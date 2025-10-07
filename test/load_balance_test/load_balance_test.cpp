#include "../space_draw/space_draw.h"
#include <random>
#include <fstream>
#include <unordered_set>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace spiritsaway::distributed_space;

void dump_json_to_file(const json& data, const std::string& path)
{
	std::ofstream ofs(path);
	ofs << data.dump();
}

std::string format_timepoint(std::uint64_t milliseconds_since_epoch)
{
	auto epoch_begin = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>();
	auto cur_timepoint = epoch_begin + std::chrono::milliseconds(milliseconds_since_epoch);
	auto cur_time_t = std::chrono::system_clock::to_time_t(cur_timepoint);

	struct tm* timeinfo;
	char buffer[80];

	timeinfo = localtime(&cur_time_t);

	strftime(buffer, sizeof(buffer), "%Y_%m_%d_%H_%M_%S", timeinfo);
	return std::string(buffer);
}

std::shared_ptr<spdlog::logger> create_logger(const std::string& name)
{

	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_level(spdlog::level::debug);
	std::string pattern = "[" + name + "] [%^%l%$] %v";
	console_sink->set_pattern(pattern);
	auto microsecondsUTC = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(name + "_" + format_timepoint(microsecondsUTC) + ".log", true);
	file_sink->set_level(spdlog::level::trace);
	auto logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{ console_sink, file_sink });
	logger->set_level(spdlog::level::trace);
	logger->flush_on(spdlog::level::warn);
	return logger;
}

std::vector<point_xz> generate_random_points(const cell_bound& cur_boundary, int num)
{
	// Seed with a real random value, if available
	std::random_device r;


	std::default_random_engine e1(r());
	std::uniform_real_distribution<double> uniform_dist_x(cur_boundary.min.x, cur_boundary.max.x);
	std::uniform_real_distribution<double> uniform_dist_z(cur_boundary.min.z, cur_boundary.max.z);
	std::vector<point_xz> result;
	for (int i = 0; i < num; i++)
	{
		point_xz temp_pos;
		temp_pos.x = uniform_dist_x(e1);
		temp_pos.z = uniform_dist_z(e1);
		result.push_back(temp_pos);
	}
	return result;
}

std::unordered_map<std::string, point_xz> generate_random_entity_load(space_cells& cur_space, const std::vector<point_xz>& temp_random_points)
{
	std::unordered_map<std::string, point_xz> result;
	auto cur_boundary = cur_space.root_node()->boundary();
	
	for (int i = 0; i < temp_random_points.size(); i++)
	{
		result[std::to_string(i)] = temp_random_points[i];
	}
	std::vector< cell_bound> point_ghost_bound;
	for (auto one_point : temp_random_points)
	{
		cell_bound temp_bound;
		temp_bound.min = one_point;
		temp_bound.max = one_point;
		temp_bound.min.x -= cur_space.ghost_radius();
		temp_bound.min.z -= cur_space.ghost_radius();
		temp_bound.max.x += cur_space.ghost_radius();
		temp_bound.max.z += cur_space.ghost_radius();
		point_ghost_bound.push_back(temp_bound);
	}
	std::vector<const space_cells::space_node*> real_cells_for_pos;
	for (auto one_point : temp_random_points)
	{
		auto cur_real_cell = cur_space.query_leaf_for_point(one_point.x, one_point.z);
		real_cells_for_pos.push_back(cur_real_cell);
	}
	for (const auto& [one_space_id, one_cell] : cur_space.all_leafs())
	{
		if (!one_cell->is_leaf_cell())
		{
			continue;
		}
		std::vector< entity_load> temp_entity_loads;
		auto cur_cell_bound = one_cell->boundary();
		float total_load = 4.0;
		for (int i = 0; i < point_ghost_bound.size(); i++)
		{
			if (point_ghost_bound[i].intersect(cur_cell_bound))
			{
				entity_load temp_entity_load;
				temp_entity_load.name = std::to_string(i);
				temp_entity_load.pos = temp_random_points[i];
				if (one_cell != real_cells_for_pos[i])
				{
					temp_entity_load.is_real = false;
					temp_entity_load.load = 0.2;
				}
				else
				{
					temp_entity_load.load = 1.0;
					temp_entity_load.is_real = true;
				}
				total_load += temp_entity_load.load;
				temp_entity_loads.push_back(temp_entity_load);
			}
		}
		cur_space.update_cell_load(one_space_id, total_load, temp_entity_loads);
	}
	return result;
}


// 执行migrate 每个cell最多迁出max_cell_migrate_num个
std::unordered_map<std::string, float> do_migrate(space_cells& cur_space, int max_cell_migrate_num, const std::unordered_map<std::string, point_xz>& entity_poses, std::shared_ptr<spdlog::logger> logger)
{
	std::unordered_map<std::string, std::string> entity_to_real_region;
	std::unordered_map<std::string, std::vector<std::string>> region_real_entities;
	std::unordered_map<std::string, float> result_game_loads;
	for (const auto& [one_space_id, one_cell] : cur_space.all_leafs())
	{
		if (!one_cell->is_leaf_cell())
		{
			continue;
		}
		
		int temp_migrate_num = 0;
		for (const auto& one_entity_load : one_cell->get_entity_loads())
		{
			if (one_entity_load.is_real && entity_poses.find(one_entity_load.name) != entity_poses.end())
			{
				if (!one_cell->boundary().cover(one_entity_load.pos.x, one_entity_load.pos.z) && temp_migrate_num < max_cell_migrate_num)
				{
					temp_migrate_num++;
					auto cur_real_cell = cur_space.query_leaf_for_point(one_entity_load.pos.x, one_entity_load.pos.z);
					if (one_space_id == cur_real_cell->space_id())
					{
						logger->info("entity {} migrate from {} to {}", one_entity_load.name, one_cell->space_id(), cur_real_cell->space_id());
					}
					else
					{
						logger->info("entity {} migrate from {} to {}", one_entity_load.name, one_cell->space_id(), cur_real_cell->space_id());
					}
					region_real_entities[cur_real_cell->space_id()].push_back(one_entity_load.name);
					entity_to_real_region[one_entity_load.name] = cur_real_cell->space_id();
				}
				else
				{
					region_real_entities[one_cell->space_id()].push_back(one_entity_load.name);
					entity_to_real_region[one_entity_load.name] = one_space_id;
				}
			}
		}
	}

	std::vector< cell_bound> point_ghost_bounds;
	std::vector<point_xz> entity_pos_vec;
	std::vector<std::string> entity_names;
	for (const auto& [one_entity_id, one_point] : entity_poses)
	{
		cell_bound temp_bound;
		temp_bound.min = one_point;
		temp_bound.max = one_point;
		temp_bound.min.x -= cur_space.ghost_radius();
		temp_bound.min.z -= cur_space.ghost_radius();
		temp_bound.max.x += cur_space.ghost_radius();
		temp_bound.max.z += cur_space.ghost_radius();
		point_ghost_bounds.push_back(temp_bound);
		entity_pos_vec.push_back(one_point);
		entity_names.push_back(one_entity_id);
	}
	std::vector<std::string> real_cells_for_pos;
	for (const auto& [one_entity_id, one_point] : entity_poses)
	{
		auto& cur_real_cell = entity_to_real_region[one_entity_id];
		if (cur_real_cell.empty()) // 对应一些被remove的节点的entity
		{
			cur_real_cell = cur_space.query_leaf_for_point(one_point.x, one_point.z)->space_id();
			region_real_entities[cur_real_cell].push_back(one_entity_id);
			logger->info("entity {} insert to {}", one_entity_id, cur_real_cell);
		}
		real_cells_for_pos.push_back(cur_real_cell);
	}
	for (const auto& [one_space_id, one_cell] : cur_space.all_leafs())
	{
		if (!one_cell->is_leaf_cell())
		{
			continue;
		}
		std::vector< entity_load> temp_entity_loads;
		auto cur_cell_bound = one_cell->boundary();
		float total_load = 4.0;
		for (int i = 0; i < point_ghost_bounds.size(); i++)
		{
			if (one_cell->space_id() == real_cells_for_pos[i] || point_ghost_bounds[i].intersect(cur_cell_bound))
			{
				entity_load temp_entity_load;
				temp_entity_load.name = entity_names[i];
				temp_entity_load.pos = entity_pos_vec[i];
				if (one_cell->space_id() == real_cells_for_pos[i])
				{
					temp_entity_load.load = 1.0;
					temp_entity_load.is_real = true;
				}
				else
				{
					temp_entity_load.is_real = false;
					temp_entity_load.load = 0.2;
				}
				total_load += temp_entity_load.load;
				temp_entity_loads.push_back(temp_entity_load);
			}
		}
		logger->info("{} update_load total_load {} temp_entity_loads {}", one_space_id, total_load, temp_entity_loads.size());
		cur_space.update_cell_load(one_space_id, total_load, temp_entity_loads);
		result_game_loads[one_cell->game_id()] += total_load + 2;
	}
	return result_game_loads;
}


std::string choose_min_load_game(const std::unordered_map<std::string, float>& game_loads, const space_cells& cur_space)
{
	if (game_loads.empty())
	{
		return {};
	}
	std::unordered_set<std::string> used_games;
	for (const auto& [one_cell_id, one_cell_ptr] : cur_space.all_leafs())
	{
		used_games.insert(one_cell_ptr->game_id());
	}
	std::string best_game;
	float best_load = 1000000;
	for (const auto& [one_game_id, one_load] : game_loads)
	{
		if (used_games.count(one_game_id))
		{
			continue;
		}
		if (one_load < best_load)
		{
			best_load = one_load;
			best_game = one_game_id;
		}
	}
	return best_game;
}
void do_balance(space_cells& cur_space, const cell_load_balance_param& lb_param, const std::unordered_map<std::string, float>& game_loads, int iteration, std::shared_ptr<spdlog::logger> cur_logger)
{
	auto cur_shrink_node = cur_space.get_best_node_to_shrink(game_loads, lb_param);
	if (cur_shrink_node)
	{
		double out_split_axis = cur_shrink_node->calc_best_shrink_new_split_pos(lb_param, cur_space.ghost_radius());
		auto cur_sibling_node = cur_shrink_node->sibling();
		cur_logger->info("{} balance at {} ", cur_shrink_node->space_id(), out_split_axis);
		cur_logger->info("before balance space {} has boundary {}", cur_shrink_node->space_id(), json(cur_shrink_node->boundary()).dump());
		cur_logger->info("before balance space {} has boundary {}", cur_sibling_node->space_id(), json(cur_sibling_node->boundary()).dump());

		cur_space.balance(out_split_axis, cur_shrink_node->parent());

		cur_logger->info("after balance space {} has boundary {}", cur_shrink_node->space_id(), json(cur_shrink_node->boundary()).dump());

		cur_logger->info("after balance space {} has boundary {}", cur_sibling_node->space_id(), json(cur_sibling_node->boundary()).dump());
		return;
	}
	auto cur_split_node = cur_space.get_best_cell_to_split(game_loads, lb_param);
	if (cur_split_node)
	{
		auto cur_split_direction = cur_split_node->calc_best_split_direction(cur_space.ghost_radius());
		auto cur_best_game = choose_min_load_game(game_loads, cur_space);
		if (!cur_best_game.empty())
		{
			auto new_space_id = "space" + std::to_string(iteration);
			cur_logger->info("{} boundary {} split_at_direction {} new_space_id {} best_game {}", cur_split_node->space_id(), json(cur_split_node->boundary()).dump(), int(cur_split_direction), new_space_id, cur_best_game);
			auto cur_split_space_id = cur_split_node->space_id();
			auto new_space_node = cur_space.split_at_direction(cur_split_space_id, cur_split_direction, new_space_id, cur_best_game);
			auto sibling_node = new_space_node->sibling();
			cur_logger->info("space {} has boundary {}", sibling_node->space_id(), json(sibling_node->boundary()).dump());
			cur_logger->info("space {} has boundary {}", new_space_node->space_id(), json(new_space_node->boundary()).dump());
			cur_space.set_ready(new_space_id);
			return;
		}
	}
	
	auto cur_remove_node = cur_space.get_best_cell_to_remove(game_loads, lb_param);
	if (cur_remove_node)
	{
		auto cur_sibling_id = cur_remove_node->sibling()->space_id();
		auto cur_remove_space_id = cur_remove_node->space_id();
		cur_logger->info("remove space {}", cur_remove_space_id);
		cur_space.merge_to_sibling(cur_remove_space_id);
		auto cur_sibling_node = cur_space.get_leaf(cur_sibling_id);
		cur_logger->info("space {} has boundary {}", cur_sibling_node->space_id(), json(cur_sibling_node->boundary()).dump());
	}
}

// 基础的等待负载 split并均衡到达稳态
void lb_case_1(const space_draw_config& draw_config, const std::string& dest_dir, std::shared_ptr<spdlog::logger> logger, const std::string& input_path)
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	cell_load_balance_param cur_lb_param;
	cur_lb_param.load_to_offset = 10;
	cur_lb_param.max_cell_load_when_remove = 6;
	cur_lb_param.min_cell_load_report_counter_when_remove = 10;
	cur_lb_param.min_cell_load_report_counter_when_shrink = 2;
	cur_lb_param.min_cell_load_report_counter_when_split = 4;
	cur_lb_param.min_cell_load_when_shrink = 20;
	cur_lb_param.min_cell_load_when_split = 40;
	cur_lb_param.min_game_load_when_split = 80;
	cur_lb_param.min_sibling_game_load_diff_when_shrink = 20;
	
	std::string root_space_id = "space1";
	std::vector<std::string> games = { "game1", "game2", "game3", "game4" };
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	std::vector<point_xz> temp_random_points;
	if (input_path.empty())
	{
		temp_random_points = generate_random_points(cur_space.root_node()->boundary(), 200);
	}
	else
	{
		auto input_point_json = load_json_file(input_path);
		input_point_json.get_to(temp_random_points);
	}
	
	auto cur_entity_poses = generate_random_entity_load(cur_space, temp_random_points);
	std::string cur_result_dir = dest_dir + "/lb_case1";
	std::filesystem::create_directories(cur_result_dir);
	dump_json_to_file(json(temp_random_points), cur_result_dir + "/" + "input_points.json");
	draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_0");
	dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_0" + ".json");
	for (int i = 0; i < 40; i++)
	{
		logger->warn("iteration {}", i);
		auto cur_game_loads = do_migrate(cur_space, 20, cur_entity_poses, logger);
		for (const auto& one_game : games)
		{
			if (!cur_game_loads.count(one_game))
			{
				cur_game_loads[one_game] = 1.0;
			}
		}
		logger->info("game_loads {}", json(cur_game_loads).dump());
		cur_space.update_load_stat(cur_game_loads);
		do_balance(cur_space, cur_lb_param, cur_game_loads, i, logger);
		logger->info("balance finish");
		draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_" + std::to_string(i+1));
		dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_" + std::to_string(i + 1) + ".json");
	}

}

// 预先划分 然后等待自动调节到稳态
void lb_case_2(const space_draw_config& draw_config, const std::string& dest_dir, std::shared_ptr<spdlog::logger> logger, const std::string& input_path)
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	cell_load_balance_param cur_lb_param;
	cur_lb_param.load_to_offset = 10;
	cur_lb_param.max_cell_load_when_remove = 6;
	cur_lb_param.min_cell_load_report_counter_when_remove = 10;
	cur_lb_param.min_cell_load_report_counter_when_shrink = 2;
	cur_lb_param.min_cell_load_report_counter_when_split = 4;
	cur_lb_param.min_cell_load_when_shrink = 20;
	cur_lb_param.min_cell_load_when_split = 40;
	cur_lb_param.min_game_load_when_split = 80;
	cur_lb_param.min_sibling_game_load_diff_when_shrink = 15;

	std::string root_space_id = "space1";
	std::vector<std::string> games = { "game0", "game1", "game2", "game3", "game4" };
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	// 预先划分为四个
	cur_space.split_x(-9000, "space1", "game1", "space1", "space2");
	cur_space.set_ready("space2");
	cur_space.split_z(10000, "space1", "game2", "space1", "space3");
	cur_space.set_ready("space3");
	cur_space.split_z(11000, "space2", "game3", "space4", "space2");
	cur_space.set_ready("space4");

	std::vector<point_xz> temp_random_points;
	if (input_path.empty())
	{
		temp_random_points = generate_random_points(cur_space.root_node()->boundary(), 200);
	}
	else
	{
		auto input_point_json = load_json_file(input_path);
		input_point_json.get_to(temp_random_points);
	}

	auto cur_entity_poses = generate_random_entity_load(cur_space, temp_random_points);
	std::string cur_result_dir = dest_dir + "/lb_case2";
	std::filesystem::create_directories(cur_result_dir);
	dump_json_to_file(json(temp_random_points), cur_result_dir + "/" + "input_points.json");
	draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_0");
	dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_0" + ".json");
	for (int i = 5; i < 40; i++)
	{
		logger->warn("iteration {}", i);
		auto cur_game_loads = do_migrate(cur_space, 20, cur_entity_poses, logger);
		for (const auto& one_game : games)
		{
			if (!cur_game_loads.count(one_game))
			{
				cur_game_loads[one_game] = 1.0;
			}
		}
		logger->info("game_loads {}", json(cur_game_loads).dump());
		cur_space.update_load_stat(cur_game_loads);
		do_balance(cur_space, cur_lb_param, cur_game_loads, i, logger);
		logger->info("balance finish");
		draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_" + std::to_string(i + 1));
		dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_" + std::to_string(i + 1) + ".json");
	}

}

// 均衡划分 然后删除一些entity 等待均衡
void lb_case_3(const space_draw_config& draw_config, const std::string& dest_dir, std::shared_ptr<spdlog::logger> logger, const std::string& input_path)
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	cell_load_balance_param cur_lb_param;
	cur_lb_param.load_to_offset = 10;
	cur_lb_param.max_cell_load_when_remove = 6;
	cur_lb_param.min_cell_load_report_counter_when_remove = 6;
	cur_lb_param.min_cell_load_report_counter_when_shrink = 2;
	cur_lb_param.min_cell_load_report_counter_when_split = 4;
	cur_lb_param.min_cell_load_when_shrink = 20;
	cur_lb_param.min_cell_load_when_split = 40;
	cur_lb_param.min_game_load_when_split = 80;
	cur_lb_param.min_sibling_game_load_diff_when_shrink = 15;

	std::string root_space_id = "space1";
	std::vector<std::string> games = { "game0", "game1", "game2", "game3", "game4" };
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	// 预先划分为四个
	cur_space.split_x(1000, "space1", "game1", "space1", "space2");
	cur_space.set_ready("space2");
	cur_space.split_z(12000, "space1", "game2", "space1", "space3");
	cur_space.set_ready("space3");
	cur_space.split_z(12000, "space2", "game3", "space4", "space2");
	cur_space.set_ready("space4");

	std::vector<point_xz> temp_random_points;
	if (input_path.empty())
	{
		temp_random_points = generate_random_points(cur_space.root_node()->boundary(), 200);
	}
	else
	{
		auto input_point_json = load_json_file(input_path);
		input_point_json.get_to(temp_random_points);
	}

	auto cur_entity_poses = generate_random_entity_load(cur_space, temp_random_points);
	std::string cur_result_dir = dest_dir + "/lb_case3";
	std::filesystem::create_directories(cur_result_dir);
	dump_json_to_file(json(temp_random_points), cur_result_dir + "/" + "input_points.json");
	draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_0");
	dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_0" + ".json");
	for (int i = 5; i < 20; i++)
	{
		logger->warn("iteration {}", i);
		auto cur_game_loads = do_migrate(cur_space, 20, cur_entity_poses, logger);
		for (const auto& one_game : games)
		{
			if (!cur_game_loads.count(one_game))
			{
				cur_game_loads[one_game] = 1.0;
			}
		}
		logger->info("game_loads {}", json(cur_game_loads).dump());
		cur_space.update_load_stat(cur_game_loads);
		do_balance(cur_space, cur_lb_param, cur_game_loads, i, logger);
		logger->info("balance finish");
		draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_" + std::to_string(i + 1));
		dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_" + std::to_string(i + 1) + ".json");
	}
	std::unordered_map<std::string, point_xz> entity_pos_copy;
	for (const auto& one_entity : cur_entity_poses)
	{
		// 删除x轴小于0的
		if (one_entity.second.x > 0)
		{
			entity_pos_copy[one_entity.first] = one_entity.second;
		}
	}
	std::swap(entity_pos_copy, cur_entity_poses);
	for (int i = 20; i < 40; i++)
	{
		logger->warn("iteration {}", i);
		auto cur_game_loads = do_migrate(cur_space, 20, cur_entity_poses, logger);
		for (const auto& one_game : games)
		{
			if (!cur_game_loads.count(one_game))
			{
				cur_game_loads[one_game] = 1.0;
			}
		}
		logger->info("game_loads {}", json(cur_game_loads).dump());
		cur_space.update_load_stat(cur_game_loads);
		do_balance(cur_space, cur_lb_param, cur_game_loads, i, logger);
		logger->info("balance finish");
		draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_" + std::to_string(i + 1));
		dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_" + std::to_string(i + 1) + ".json");
	}
}

// 均衡划分 然后删除绝大部分entity 等待删除
void lb_case_4(const space_draw_config& draw_config, const std::string& dest_dir, std::shared_ptr<spdlog::logger> logger, const std::string& input_path)
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	cell_load_balance_param cur_lb_param;
	cur_lb_param.load_to_offset = 10;
	cur_lb_param.max_cell_load_when_remove = 20;
	cur_lb_param.min_cell_load_report_counter_when_remove = 6;
	cur_lb_param.min_cell_load_report_counter_when_shrink = 2;
	cur_lb_param.min_cell_load_report_counter_when_split = 4;
	cur_lb_param.min_cell_load_when_shrink = 20;
	cur_lb_param.min_cell_load_when_split = 40;
	cur_lb_param.min_game_load_when_split = 80;
	cur_lb_param.min_sibling_game_load_diff_when_shrink = 15;

	std::string root_space_id = "space1";
	std::vector<std::string> games = { "game0", "game1", "game2", "game3", "game4" };
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	// 预先划分为四个
	cur_space.split_x(1000, "space1", "game1", "space1", "space2");
	cur_space.set_ready("space2");
	cur_space.split_z(12000, "space1", "game2", "space1", "space3");
	cur_space.set_ready("space3");
	cur_space.split_z(12000, "space2", "game3", "space4", "space2");
	cur_space.set_ready("space4");

	std::vector<point_xz> temp_random_points;
	if (input_path.empty())
	{
		temp_random_points = generate_random_points(cur_space.root_node()->boundary(), 200);
	}
	else
	{
		auto input_point_json = load_json_file(input_path);
		input_point_json.get_to(temp_random_points);
	}

	auto cur_entity_poses = generate_random_entity_load(cur_space, temp_random_points);
	std::string cur_result_dir = dest_dir + "/lb_case4";
	std::filesystem::create_directories(cur_result_dir);
	dump_json_to_file(json(temp_random_points), cur_result_dir + "/" + "input_points.json");
	draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_0");
	dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_0" + ".json");
	for (int i = 5; i < 10; i++)
	{
		logger->warn("iteration {}", i);
		auto cur_game_loads = do_migrate(cur_space, 20, cur_entity_poses, logger);
		for (const auto& one_game : games)
		{
			if (!cur_game_loads.count(one_game))
			{
				cur_game_loads[one_game] = 1.0;
			}
		}
		logger->info("game_loads {}", json(cur_game_loads).dump());
		cur_space.update_load_stat(cur_game_loads);
		do_balance(cur_space, cur_lb_param, cur_game_loads, i, logger);
		logger->info("balance finish");
		draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_" + std::to_string(i + 1));
		dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_" + std::to_string(i + 1) + ".json");
	}
	std::unordered_map<std::string, point_xz> entity_pos_copy;
	for (const auto& one_entity : cur_entity_poses)
	{
		// 删除x轴小于0的
		if (one_entity.second.x > 10000)
		{
			entity_pos_copy[one_entity.first] = one_entity.second;
		}
	}
	std::swap(entity_pos_copy, cur_entity_poses);
	for (int i = 10; i < 40; i++)
	{
		logger->warn("iteration {}", i);
		auto cur_game_loads = do_migrate(cur_space, 20, cur_entity_poses, logger);
		for (const auto& one_game : games)
		{
			if (!cur_game_loads.count(one_game))
			{
				cur_game_loads[one_game] = 1.0;
			}
		}
		logger->info("game_loads {}", json(cur_game_loads).dump());
		cur_space.update_load_stat(cur_game_loads);
		do_balance(cur_space, cur_lb_param, cur_game_loads, i, logger);
		logger->info("balance finish");
		draw_cell_region(cur_space, draw_config, cur_result_dir, "iter_" + std::to_string(i + 1));
		dump_json_to_file(cur_space.encode(), cur_result_dir + "/" + "iter_" + std::to_string(i + 1) + ".json");
	}
}



int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		std::cout << "should provide draw_config json" << std::endl;
		return 1;
	}
	
	auto draw_config_json = load_json_file(argv[1]);
	if (draw_config_json.is_null())
	{
		std::cout << "fail to load json from " << argv[1] << std::endl;
		return 1;
	}
	space_draw_config cur_draw_config;
	try
	{
		draw_config_json.get_to(cur_draw_config);
	}
	catch (std::exception& e)
	{
		std::cout << "fail to decode space_draw_config from " << draw_config_json << " exception " << e.what() << std::endl;
		return 1;
	}
	auto microsecondsUTC = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	auto cur_folder_name = "dump_space_" + format_timepoint(microsecondsUTC);
	auto cur_logger = create_logger("load_balance");
	//cur_logger->info("lb_case_1");
	//lb_case_1(cur_draw_config, cur_folder_name, cur_logger, "");
	//cur_logger->info("lb_case_2");
	//lb_case_2(cur_draw_config, cur_folder_name, cur_logger, "");
	//cur_logger->info("lb_case_3");
	//lb_case_3(cur_draw_config, cur_folder_name, cur_logger, "");
	cur_logger->info("lb_case_4");
	lb_case_4(cur_draw_config, cur_folder_name, cur_logger, "");
	return 1;
}