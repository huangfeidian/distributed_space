#include "../space_draw/space_draw.h"
#include <random>
#include <fstream>
using namespace spiritsaway::utility;

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

void generate_random_entity_load(space_cells& cur_space, int num)
{
	auto cur_boundary = cur_space.root_cell()->boundary();
	auto temp_random_points = generate_random_points(cur_boundary, num);
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
	std::vector<const space_cells::cell_node*> real_cells_for_pos;
	for (auto one_point : temp_random_points)
	{
		auto cur_real_cell = cur_space.query_point_region(one_point.x, one_point.z);
		real_cells_for_pos.push_back(cur_real_cell);
	}
	for (const auto& [one_space_id, one_cell] : cur_space.all_cells())
	{
		if (!one_cell->is_leaf())
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
}
void dump_json_to_file(const json& data, const std::string& path)
{
	std::ofstream ofs(path);
	ofs << data.dump();
}

// 执行migrate 每个cell最多迁出max_cell_migrate_num个
void do_migrate(space_cells& cur_space, int max_cell_migrate_num)
{
	std::unordered_map<std::string, std::string> entity_to_real_region;
	std::unordered_map<std::string, std::vector<std::string>> region_real_entities;
	std::unordered_map<std::string, point_xz> entity_poses;
	for (const auto& [one_space_id, one_cell] : cur_space.all_cells())
	{
		if (!one_cell->is_leaf())
		{
			continue;
		}
		
		int temp_migrate_num = 0;
		for (const auto& one_entity_load : one_cell->get_entity_loads())
		{
			if (one_entity_load.is_real)
			{
				entity_poses[one_entity_load.name] = one_entity_load.pos;
				if (!one_cell->boundary().cover(one_entity_load.pos.x, one_entity_load.pos.z) && temp_migrate_num < max_cell_migrate_num)
				{
					temp_migrate_num++;
					auto cur_real_cell = cur_space.query_point_region(one_entity_load.pos.x, one_entity_load.pos.z);
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
	for (const auto& [one_entity_id, one_space_id] : entity_to_real_region)
	{
		auto cur_pos = entity_poses[one_entity_id];
	}
	std::vector< cell_bound> point_ghost_bound;
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
		point_ghost_bound.push_back(temp_bound);
		entity_pos_vec.push_back(one_point);
		entity_names.push_back(one_entity_id);
	}
	std::vector<std::string> real_cells_for_pos;
	for (const auto& [one_entity_id, one_point] : entity_poses)
	{
		auto cur_real_cell = entity_to_real_region[one_entity_id];
		real_cells_for_pos.push_back(cur_real_cell);
	}
	for (const auto& [one_space_id, one_cell] : cur_space.all_cells())
	{
		if (!one_cell->is_leaf())
		{
			continue;
		}
		std::vector< entity_load> temp_entity_loads;
		auto cur_cell_bound = one_cell->boundary();
		float total_load = 4.0;
		for (int i = 0; i < point_ghost_bound.size(); i++)
		{
			if (one_cell->space_id() == real_cells_for_pos[i] || point_ghost_bound[i].intersect(cur_cell_bound))
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
		cur_space.update_cell_load(one_space_id, total_load, temp_entity_loads);
	}
}

void do_balance(space_cells& cur_space, const cell_load_balance_param& lb_param, const std::unordered_map<std::string, float>& game_loads)
{
	auto cur_split_node = cur_space.get_best_cell_to_split(game_loads, lb_param);
	if (cur_split_node)
	{
		//TODO
	}
	// TODO
}
void lb_case_1()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	std::vector<std::string> games = { "game1", "game2", "game3", "game4" };
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	generate_random_entity_load(cur_space, 200);

}