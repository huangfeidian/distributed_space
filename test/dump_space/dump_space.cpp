#include "space_cells.h"
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

std::vector<point_xz> generate_random_points(const cell_bound& cur_boundary , int num)
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
// 单一space 无entity
void dump_case_1()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	auto cur_space_json = cur_space.encode();
	dump_json_to_file(cur_space_json, "case_1.json");
}

// 单space 有entity
void dump_case_2()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	generate_random_entity_load(cur_space, 50);
	auto cur_space_json = cur_space.encode();
	dump_json_to_file(cur_space_json, "case_2.json");
}

// 划分一次 无entity
void dump_case_3()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	cur_space.split_x(8000, root_space_id, "game1", root_space_id, "space2");
	cur_space.set_ready("space2");
	auto cur_space_json = cur_space.encode();
	dump_json_to_file(cur_space_json, "case_3.json");
}

// 划分一次 有entity
void dump_case_4()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	cur_space.split_x(8000, root_space_id, "game1", root_space_id, "space2");
	cur_space.set_ready("space2");
	generate_random_entity_load(cur_space, 50);
	auto cur_space_json = cur_space.encode();
	dump_json_to_file(cur_space_json, "case_4.json");
}

// 划分两次 有entity
void dump_case_5()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	cur_space.split_x(8000, root_space_id, "game1", root_space_id, "space2");
	cur_space.set_ready("space2");
	cur_space.split_z(12000, root_space_id, "game2", root_space_id, "space3");
	cur_space.set_ready("space3");
	generate_random_entity_load(cur_space, 50);
	auto cur_space_json = cur_space.encode();
	dump_json_to_file(cur_space_json, "case_5.json");
}

// 划分三次 有entity
void dump_case_6()
{
	cell_bound temp_bound;
	temp_bound.min.x = -10000;
	temp_bound.max.x = 15000;
	temp_bound.min.z = 8000;
	temp_bound.max.z = 17000;
	std::string root_space_id = "space1";
	space_cells cur_space(temp_bound, "game0", root_space_id, 400);
	cur_space.set_ready(root_space_id);
	cur_space.split_x(8000, root_space_id, "game1", root_space_id, "space2");
	cur_space.set_ready("space2");
	cur_space.split_z(12000, root_space_id, "game2", root_space_id, "space3");
	cur_space.set_ready("space3");
	cur_space.split_z(13000, "space2", "game4", "space2", "space4");
	cur_space.set_ready("space4");
	generate_random_entity_load(cur_space, 50);
	auto cur_space_json = cur_space.encode();
	dump_json_to_file(cur_space_json, "case_6.json");
}

int main()
{
	dump_case_1();
	dump_case_2();
	dump_case_3();
	dump_case_4();
	dump_case_5();
	dump_case_6();
	return 1;
}