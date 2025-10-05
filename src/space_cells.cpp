#include "space_cells.h"

namespace spiritsaway::utility
{
	bool cell_bound::intersect(const cell_bound& other) const
	{
		if(min.x >= other.max.x)
		{
			return false;
		}
		if(max.x <= other.min.x)
		{
			return false;
		}
		if(min.z >= other.max.z)
		{
			return false;
		}
		if(max.z <= other.min.z)
		{
			return false;
		}
		return true;
	}
	space_cells::space_cells(const cell_bound& bound, const std::string& game_id, const std::string& space_id, double in_ghost_radius)
	: m_root_cell(new cell_node(bound, game_id, space_id, nullptr))
	{
		m_cells[space_id] = m_root_cell;
		m_master_cell_id = space_id;
		m_ghost_radius = in_ghost_radius;
	}

	const space_cells::cell_node* space_cells::cell_node::sibling() const
	{
		if(!m_parent)
		{
			return nullptr;
		}
		if(m_parent->m_children[0] == this)
		{
			return m_parent->m_children[1];
		}
		else
		{
			return m_parent->m_children[0];
		}
	}
	void space_cells::cell_node::set_ready()
	{
		m_ready = true;
	}

	float space_cells::cell_node::get_smoothed_load() const
	{
		float square_sum = 0;
		float total_weights = 0.01f;
		float current_weight = 1.0f;
		std::uint32_t back_iter_idx = m_cell_load_report_counter;
		while(back_iter_idx > 0 && m_cell_load_report_counter - back_iter_idx < m_cell_loads.size() )
		{
			auto cur_period_load = m_cell_loads[back_iter_idx % m_cell_loads.size()];
			if(cur_period_load == 0)
			{
				break;
			}
			square_sum += cur_period_load * cur_period_load * current_weight;
			total_weights += current_weight;
			back_iter_idx--;
			current_weight -= 0.2f;
		}
		
		return std::sqrt(square_sum/total_weights);
	}
	
	void space_cells::cell_node::update_load(float cur_load, const std::vector<entity_load>& new_entity_loads)
	{
		m_cell_load_report_counter++;
		m_cell_loads[m_cell_load_report_counter % m_cell_loads.size()] = cur_load;
		m_entity_loads = new_entity_loads;
	}

	space_cells::cell_node* space_cells::cell_node::split_x(double x, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id, const std::string& new_parent_space_id)
	{
		if(!is_leaf())
		{
			return nullptr;
		}
		if(x <= m_boundary.min.x || x >= m_boundary.max.x)
		{
			return nullptr;
		}
		if(m_space_id != left_space_id && m_space_id != right_space_id)
		{
			return nullptr;
		}
		cell_bound left_boundary, right_boundary;
		left_boundary = right_boundary = m_boundary;
		left_boundary.max.x = x;
		right_boundary.min.x = x;
		m_is_split_x = true;
		m_children[0] = new cell_node(left_boundary, m_game_id, left_space_id, this);
		m_children[1] = new cell_node(right_boundary, m_game_id, right_space_id, this);
		auto pre_space_id = m_space_id;
		
		int master_cell_idx = 0;
		if (pre_space_id != left_space_id)
		{
			master_cell_idx = 1;
		}
		on_split(master_cell_idx);
		m_children[1 - master_cell_idx]->m_game_id = new_space_game_id;
		m_space_id = new_parent_space_id;
		return m_children[1 - master_cell_idx];
	}

	void space_cells::cell_node::on_split(int master_child_index)
	{
		m_children[master_child_index]->m_cell_loads[1] = get_latest_load();
		m_children[master_child_index]->m_cell_load_report_counter = 1;
		m_children[master_child_index]->m_entity_loads = std::move(m_entity_loads);
		m_children[master_child_index]->set_ready();
	}
	space_cells::cell_node* space_cells::cell_node::split_z(double z, const std::string& new_space_game_id, const std::string& low_space_id, const std::string& high_space_id, const std::string& new_parent_space_id)
	{
		if(!is_leaf())
		{
			return nullptr;
		}
		if(z <= m_boundary.min.z || z >= m_boundary.max.z)
		{
			return nullptr;
		}
		if(m_space_id != low_space_id && m_space_id != high_space_id)
		{
			return nullptr;
		}
		m_is_split_x = false;
		cell_bound low_boundary, high_boundary;
		low_boundary = high_boundary = m_boundary;
		low_boundary.max.z = z;
		high_boundary.min.z = z;
		m_children[0] = new cell_node(low_boundary, m_game_id, low_space_id, this);
		m_children[1] = new cell_node(high_boundary, m_game_id, high_space_id, this);
		auto pre_space_id = m_space_id;
		
		int master_cell_idx = 0;
		if (pre_space_id != low_space_id)
		{
			master_cell_idx = 1;
		}
		on_split(master_cell_idx);
		m_children[1 - master_cell_idx]->m_game_id = new_space_game_id;
		m_space_id = new_parent_space_id;
		return m_children[1 - master_cell_idx];
	}
	json space_cells::cell_node::encode() const
	{
		json result;
		result["space_id"] = m_space_id;
		result["game_id"] = m_game_id;
		result["bound"] = m_boundary;
		if(m_parent)
		{
			result["parent"] = m_parent->m_space_id;
		}
		else
		{
			result["parent"] = std::string();
		}
		std::array<std::string, 2> children_ids;
		children_ids[0] = m_children[0]? m_children[0]->m_space_id:std::string{};
		children_ids[1] = m_children[1]? m_children[1]->m_space_id:std::string{};
		result["children"] = children_ids;
		result["ready"] = ready();
		if (!is_leaf())
		{
			result["is_split_x"] = m_is_split_x;
		}
		else
		{
			result["entity_loads"] = m_entity_loads;
			result["cell_loads"] = m_cell_loads;
			result["cell_load_counter"] = m_cell_load_report_counter;
		}
		
		return result;
	}
	bool space_cells::cell_node::set_child(int index, cell_node* new_child)
	{
		if(index != 0 && index != 1)
		{
			return false;
		}
		if(m_children[index])
		{
			return false;
		}
		m_children[index] = new_child;
		return true;
	}

	float space_cells::cell_node::get_latest_load() const
	{
		return m_cell_loads[m_cell_load_report_counter % m_cell_loads.size()];
	}
	bool space_cells::cell_node::balance(double split_v)
	{
		if(is_leaf())
		{
			return false;
		}
		if(m_is_split_x)
		{
			if(m_children[0]->m_boundary.min.x == m_children[1]->m_boundary.min.x)
			{
				return false;
			}
			if(split_v <= m_children[0]->m_boundary.min.x || split_v >= m_children[1]->m_boundary.max.x)
			{
				return false;
			}
			m_children[0]->m_boundary.max.x = split_v;
			m_children[1]->m_boundary.min.x = split_v;
		}
		else
		{
			if(m_children[0]->m_boundary.min.z == m_children[1]->m_boundary.min.z)
			{
				return false;
			}
			if(split_v <= m_children[0]->m_boundary.min.z || split_v >= m_children[1]->m_boundary.max.z)
			{
				return false;
			}
			m_children[0]->m_boundary.max.z = split_v;
			m_children[1]->m_boundary.min.z = split_v;
		}
		m_children[0]->m_cell_loads[1] = m_children[0]->get_latest_load();
		m_children[0]->m_cell_load_report_counter = 1;
		
		m_children[1]->m_cell_loads[1] = m_children[1]->get_latest_load();
		m_children[1]->m_cell_load_report_counter = 1;
		
		return true;
	}
	
	void space_cells::cell_node::merge_to_child(const std::string& dest)
	{
		m_entity_loads.clear();
		m_cell_load_report_counter = 1;
		m_space_id = dest;
		if (m_children[0]->space_id() == dest)
		{
			m_game_id = m_children[0]->game_id();
			m_entity_loads = std::move(m_children[0]->m_entity_loads);
			m_cell_loads[1] = m_children[0]->get_latest_load();
		}
		else
		{
			m_game_id = m_children[1]->game_id();
			m_entity_loads = std::move(m_children[1]->m_entity_loads);
			m_cell_loads[1] = m_children[1]->get_latest_load();
		}
		m_ready = true;
		delete m_children[0];
		delete m_children[1];
		m_children[0] = nullptr;
		m_children[1] = nullptr;
	}

	
	std::string space_cells::merge_to_sibling(const std::string& space_id)
	{
		auto remove_node_iter = m_cells.find(space_id);
		if(remove_node_iter == m_cells.end())
		{
			return {};
		}
		auto remove_node = remove_node_iter->second;
		if(!remove_node->is_leaf())
		{
			return {};
		}
		if (!remove_node->ready())
		{
			return {};
		}
		if (remove_node->game_id() == m_master_cell_id)
		{
			return {};
		}
		auto sibling_node = remove_node->sibling();
		if (!sibling_node || !sibling_node->is_leaf())
		{
			return {};
		}
		if (!sibling_node->ready() || !remove_node->ready())
		{
			return {};
		}
		
		auto cur_parent = remove_node->parent();
		if(!cur_parent)
		{
			return {};
		}
		std::string remove_node_game_id = remove_node->game_id();
		cur_parent->merge_to_child(sibling_node->space_id());
		
		m_cells.erase(remove_node_iter);
		m_cells[space_id] = cur_parent;
		return remove_node_game_id;
	}
	bool space_cells::check_valid_space_id(const std::string& space_id) const
	{
		if (space_id.empty())
		{
			return false;
		}
		return !std::all_of(space_id.begin(), space_id.end(), ::isdigit);
	}

	const space_cells::cell_node* space_cells::split_x(double x, const std::string& origin_space_id, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id)
	{
		if(!check_valid_space_id(left_space_id) || ! check_valid_space_id(right_space_id))
		{
			return nullptr;
		}
		auto dest_node_iter = m_cells.find(origin_space_id);
		if(dest_node_iter == m_cells.end())
		{
			return nullptr;
		}
		auto dest_node = dest_node_iter->second;
		if(!dest_node->is_leaf())
		{
			return nullptr;
		}
		m_temp_node_counter++;
		auto result = dest_node->split_x(x, new_space_game_id, left_space_id, right_space_id, std::to_string(m_temp_node_counter));
		if(!result)
		{
			return nullptr;
		}
		m_cells.erase(dest_node_iter);
		for(auto one_child: dest_node->children())
		{
			m_cells[one_child->space_id()] = one_child;
		}
		return result;
		
	}

	const space_cells::cell_node* space_cells::split_z(double z, const std::string& origin_space_id, const std::string& new_space_game_id,  const std::string& low_space_id, const std::string& high_space_id)
	{
		if(!check_valid_space_id(low_space_id) || ! check_valid_space_id(high_space_id))
		{
			return nullptr;
		}
		auto dest_node_iter = m_cells.find(origin_space_id);
		if(dest_node_iter == m_cells.end())
		{
			return nullptr;
		}
		auto dest_node = dest_node_iter->second;
		if(!dest_node->is_leaf())
		{
			return nullptr;
		}
		m_temp_node_counter++;
		auto result = dest_node->split_z(z, new_space_game_id, low_space_id, high_space_id, std::to_string(m_temp_node_counter));
		if(!result)
		{
			return nullptr;
		}
		m_cells.erase(dest_node_iter);
		for(auto one_child: dest_node->children())
		{
			m_cells[one_child->space_id()] = one_child;
		}
		return result;
		
	}

	std::vector<const space_cells::cell_node*> space_cells::query_intersect(const cell_bound& bound) const
	{
		std::vector<const cell_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_cell);
		std::vector<const space_cells::cell_node*> result;
		while(!temp_query_buffer.empty())
		{
			auto temp_top = temp_query_buffer.back();
			temp_query_buffer.pop_back();
			if(!temp_top->boundary().intersect(bound))
			{
				continue;
			}
			if(temp_top->is_leaf())
			{
				result.push_back(temp_top);
			}
			else
			{
				temp_query_buffer.push_back(temp_top->children()[0]);
				temp_query_buffer.push_back(temp_top->children()[1]);
			}
		}
		return result;
	}

	const space_cells::cell_node* space_cells::query_point_region(double x, double z) const
	{
		std::vector<const cell_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_cell);
		while(!temp_query_buffer.empty())
		{
			auto temp_top = temp_query_buffer.back();
			temp_query_buffer.pop_back();
			if(!temp_top->boundary().cover(x, z))
			{
				continue;
			}
			if(temp_top->is_leaf())
			{
				// 在还没有ready的情况下 使用其sibling节点
				if(temp_top->ready())
				{
					return temp_top;
				}
				else
				{
					return temp_top->sibling();
				}
			}
			else
			{
				temp_query_buffer.push_back(temp_top->children()[0]);
				temp_query_buffer.push_back(temp_top->children()[1]);
			}
		}
		return nullptr;
	}

	json space_cells::encode() const
	{
		json result;
		json::array_t cell_jsons;
		cell_jsons.reserve(8);
		std::vector<const cell_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_cell);
		while(!temp_query_buffer.empty())
		{
			auto temp_top = temp_query_buffer.back();
			temp_query_buffer.pop_back();
			cell_jsons.push_back(temp_top->encode());
			if(temp_top->is_leaf())
			{
				continue;
			}
			else
			{
				temp_query_buffer.push_back(temp_top->children()[0]);
				temp_query_buffer.push_back(temp_top->children()[1]);
			}
		}
		result["master_cell_id"] = m_master_cell_id;
		result["cells"] = cell_jsons;
		result["ghost_radius"] = m_ghost_radius;
		return result;
	}

	bool space_cells::decode(const json& data)
	{
		std::string parent_space_id;
		std::array<std::string, 2> children_ids;
		cell_bound temp_bound;
		std::string temp_space_id;
		std::string temp_game_id;
		std::unordered_map<std::string, int> children_indexes;
		bool temp_ready;
		json::array_t cell_jsons;
		m_cells.clear();
		if (m_root_cell)
		{
			delete m_root_cell;
			m_root_cell = nullptr;
		}
		try
		{
			data.at("cells").get_to(cell_jsons);
			data.at("master_cell_id").get_to(m_master_cell_id);
			data.at("ghost_radius").get_to(m_ghost_radius);
		}
		catch(const std::exception& e)
		{
			(void)e;
			return false;
		}
		
		for(const auto& one_node: cell_jsons)
		{
			try
			{
				one_node.at("bound").get_to(temp_bound);
				one_node.at("children").get_to(children_ids);
				one_node.at("parent").get_to(parent_space_id);
				one_node.at("space_id").get_to(temp_space_id);
				one_node.at("game_id").get_to(temp_game_id);
				one_node.at("ready").get_to(temp_ready);
				cell_node* parent_node = nullptr;

				if(!parent_space_id.empty())
				{
					auto temp_iter = m_cells.find(parent_space_id);
					if(temp_iter == m_cells.end())
					{
						return false;
					}
					parent_node = temp_iter->second;
				}
				
				auto new_node = new cell_node(temp_bound, temp_game_id, temp_space_id, parent_node);
				if (children_ids[0].empty())
				{
					one_node.at("cell_loads").get_to(new_node->m_cell_loads);
					one_node.at("entity_loads").get_to(new_node->m_entity_loads);
					one_node.at("cell_load_counter").get_to(new_node->m_cell_load_report_counter);
				}
				else
				{
					one_node.at("is_split_x").get_to(new_node->m_is_split_x);
				}
				if(temp_ready)
				{
					new_node->set_ready();
				}
				m_cells[temp_space_id] = new_node;
				children_indexes[children_ids[0]] = 0;
				children_indexes[children_ids[1]] = 1;
				auto temp_iter = children_indexes.find(temp_space_id);
				if(temp_iter != children_indexes.end())
				{
					if(!parent_node->set_child(temp_iter->second, new_node))
					{
						return false;
					}
				}
				if(parent_node == nullptr)
				{
					m_root_cell = new_node;
				}
			}
			catch(const std::exception& e)
			{
				(void)e;
				return false;
			}
			
		}
		return true;

	}
	bool space_cells::balance(double split_v, const std::string& cell_id)
	{
		auto cur_node_iter = m_cells.find(cell_id);
		if(cur_node_iter == m_cells.end())
		{
			return false;
		}
		auto cur_sibling = cur_node_iter->second->sibling();
		if (!cur_sibling || !cur_sibling->is_leaf())
		{
			return false;
		}
		cur_node_iter->second->m_parent->balance(split_v);
		return true;
		
	}
	std::vector<std::string> space_cells::all_child_space_except(const std::string& except_space) const
	{
		std::vector<std::string> result;
		result.reserve(m_cells.size() /2 + 2);
		for(const auto& one_cell: m_cells)
		{
			if(!one_cell.second->is_leaf())
			{
				continue;
			}
			if(one_cell.first == except_space)
			{
				continue;
			}
			result.push_back(one_cell.first);
		}
		return result;
	}

	bool space_cells::set_ready(const std::string& cell_id)
	{
		auto cur_node_iter = m_cells.find(cell_id);
		if(cur_node_iter == m_cells.end())
		{
			return false;
		}
		cur_node_iter->second->set_ready();
		return true;
	}

	space_cells::~space_cells()
	{
		for(auto one_pair: m_cells)
		{
			delete one_pair.second;
		}
		m_cells.clear();
		m_root_cell = nullptr;
	}

	void space_cells::update_cell_load(const std::string& cell_space_id, float cell_load, const std::vector<entity_load>& new_entity_loads)
	{
		auto cur_node_iter = m_cells.find(cell_space_id);
		if(cur_node_iter == m_cells.end())
		{
			return;
		}
		if (cur_node_iter->second->is_leaf())
		{
			cur_node_iter->second->update_load(cell_load, new_entity_loads);
		}

	}

	bool space_cells::cell_node::calc_offset_axis(float load_to_offset,  double& out_split_axis, float& offseted_load, float ghost_radius) const
	{
		if (!is_leaf())
		{
			return false;
		}
		
		auto cur_sibling = sibling();
		if (!cur_sibling)
		{
			return false;
		}
		if (m_entity_loads.empty())
		{
			return false;
		}
		bool is_x = m_parent->m_is_split_x;

		int axis = 0; // 0 for x 1 for z
		bool should_reverse = false;
		float split_boundary = 0;
		if (is_x)
		{
			if (m_boundary.min.x < cur_sibling->boundary().min.x)
			{
				should_reverse = true;
				split_boundary = m_boundary.max.x - 4* ghost_radius;
			}
			else
			{
				split_boundary = m_boundary.min.x + 4 * ghost_radius;
			}
		}
		else
		{
			axis = 1;
			if (m_boundary.min.z < cur_sibling->boundary().min.z)
			{
				should_reverse = true;
				split_boundary = m_boundary.max.z - 4 * ghost_radius;
			}
			else
			{
				split_boundary = m_boundary.min.z + 4 * ghost_radius;
			}
		}
		
		
		auto sorted_entity_loads = m_entity_loads;
		std::sort(sorted_entity_loads.begin(), sorted_entity_loads.end(), [axis](const entity_load& a, const entity_load& b)
			{
				return a.pos[axis] < b.pos[axis];
			});
		if (should_reverse)
		{
			std::reverse(sorted_entity_loads.begin(), sorted_entity_loads.end());
		}
		
		float accumulated_load = 0;
		double pre_split_candidate = sorted_entity_loads[0].pos[axis] - 100;
		

		for (const auto& one_entity_load : sorted_entity_loads)
		{
			if (one_entity_load.pos[axis] != pre_split_candidate)
			{
				bool boundary_check_result = true;
				if (should_reverse)
				{
					boundary_check_result = one_entity_load.pos[axis] > split_boundary;
				}
				else
				{
					boundary_check_result = one_entity_load.pos[axis] < split_boundary;
				}
				if (!boundary_check_result || accumulated_load >= load_to_offset)
				{
					offseted_load = accumulated_load;
					out_split_axis = pre_split_candidate;
					out_split_axis += should_reverse ? -1 : 1;
					return true;
				}
				pre_split_candidate = one_entity_load.pos[axis];
			}
			accumulated_load += one_entity_load.load;
		}
		return false;
	}

	const space_cells::cell_node* space_cells::get_best_cell_to_split(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param) const
	{
		const space_cells::cell_node* best_result = nullptr;
		for (const auto& [one_cell_id, one_cell_node] : m_cells)
		{
			if (!one_cell_node->is_leaf())
			{
				continue;
			}
			if (one_cell_node->cell_load_report_counter() <= lb_param.min_cell_load_report_counter_when_split)
			{
				continue;
			}
			auto cur_boundary = one_cell_node->boundary();
			if ((cur_boundary.max.x - cur_boundary.min.x < 8 * m_ghost_radius) && (cur_boundary.max.z - cur_boundary.min.z < 8 * m_ghost_radius))
			{
				continue;
			}
			if (one_cell_node->get_smoothed_load() < lb_param.min_cell_load_when_split)
			{
				continue;
			}
			auto temp_game_iter = game_loads.find(one_cell_node->game_id());
			if (temp_game_iter == game_loads.end())
			{
				continue;
			}
			if (temp_game_iter->second < lb_param.min_game_load_when_split)
			{
				continue;
			}
			if (!best_result || one_cell_node->get_smoothed_load() >= best_result->get_smoothed_load())
			{
				best_result = one_cell_node;
			}

		}
		return best_result;
	}

	const space_cells::cell_node* space_cells::get_best_cell_to_remove(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param)
	{
		const space_cells::cell_node* best_result = nullptr;
		for (const auto& [one_cell_id, one_cell_node] : m_cells)
		{
			if (!one_cell_node->is_leaf())
			{
				continue;
			}
			if (one_cell_node->cell_load_report_counter() <= lb_param.min_cell_load_report_counter_when_remove)
			{
				continue;
			}
			auto cur_sibling = one_cell_node->sibling();
			if (!cur_sibling || cur_sibling->cell_load_report_counter() <= lb_param.min_cell_load_report_counter_when_remove)
			{
				continue;
			}
			if (one_cell_node->get_smoothed_load() > lb_param.max_cell_load_when_remove)
			{
				continue;
			}
			
			if (!best_result || one_cell_node->get_smoothed_load() < best_result->get_smoothed_load())
			{
				best_result = one_cell_node;
			}

		}
		return best_result;
	}

	const space_cells::cell_node* space_cells::get_best_cell_to_shrink(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param)
	{
		const space_cells::cell_node* best_result = nullptr;
		for (const auto& [one_cell_id, one_cell_node] : m_cells)
		{
			if (!one_cell_node->is_leaf())
			{
				continue;
			}
			if (one_cell_node->cell_load_report_counter() <= lb_param.min_cell_load_report_counter_when_shrink)
			{
				continue;
			}
			auto cur_sibling = one_cell_node->sibling();
			if (!cur_sibling || cur_sibling->cell_load_report_counter() <= lb_param.min_cell_load_report_counter_when_shrink)
			{
				continue;
			}
			auto cur_node_smoothed_load = one_cell_node->get_smoothed_load();
			if (cur_node_smoothed_load < lb_param.min_cell_load_when_shrink)
			{
				continue;
			}

			auto temp_game_iter = game_loads.find(one_cell_node->game_id());
			if (temp_game_iter == game_loads.end())
			{
				continue;
			}
			auto sibling_game_iter = game_loads.find(one_cell_node->sibling()->game_id());
			if (sibling_game_iter == game_loads.end())
			{
				continue;
			}
			if(sibling_game_iter->second + lb_param.load_to_offset > temp_game_iter->second)
			{
				continue;
			}
			if (sibling_game_iter->second >= lb_param.max_sibling_cell_load_when_shrink)
			{
				continue;
			}

			if (!best_result || cur_node_smoothed_load > best_result->get_smoothed_load())
			{
				best_result = one_cell_node;
			}

		}
		return best_result;
	}

	cell_split_direction space_cells::cell_node::calc_best_split_direction(float ghost_radius) const
	{
		if (m_entity_loads.empty())
		{
			// 选择最长边的一个方向

			if (m_boundary.max.x - m_boundary.min.x > m_boundary.max.z - m_boundary.min.z)
			{
				return cell_split_direction::left_x;
			}
			else
			{
				return cell_split_direction::low_z;
			}

		}
		else
		{
			std::array<float, 4> split_gains;
			std::fill(split_gains.begin(), split_gains.end(), 0);
			auto entity_loads_copy = m_entity_loads;
			float temp_acc_loads = 0;
			for (int i = 0; i < 1; i++)
			{
				std::sort(entity_loads_copy.begin(), entity_loads_copy.end(), [i](const entity_load& a, const entity_load& b)
					{
						return a.pos[i] < b.pos[i];
					});

				for (const auto& one_load : entity_loads_copy)
				{
					if (one_load.pos[i] > m_boundary.min[i] + 4 * ghost_radius)
					{
						break;
					}
					else
					{
						temp_acc_loads += one_load.load;
					}
				}
				split_gains[i*2] = temp_acc_loads;
				std::reverse(entity_loads_copy.begin(), entity_loads_copy.end());
				temp_acc_loads = 0;
				for (const auto& one_load : entity_loads_copy)
				{
					if (one_load.pos[i] + 4 * ghost_radius < m_boundary.max[i])
					{
						break;
					}
					else
					{
						temp_acc_loads += one_load.load;
					}
				}
				split_gains[i*2 + 1] = temp_acc_loads;
			}
			int best_dir = 0;
			float best_gain = split_gains[0];
			for (int i = 1; i < 4; i++)
			{
				if (split_gains[i] > best_gain)
				{
					best_gain = split_gains[i];
					best_dir = i;
				}
			}
			return cell_split_direction(best_dir);
		}
	}

	const space_cells::cell_node* space_cells::split_at_direction(const std::string& origin_space_id, cell_split_direction split_direction, const std::string& new_space_id, const std::string& new_space_game_id)
	{
		auto cur_cell_iter = m_cells.find(origin_space_id);
		if (cur_cell_iter == m_cells.end())
		{
			return nullptr;
		}
		auto cur_cell = cur_cell_iter->second;
		if (!cur_cell->ready() || cur_cell->cell_load_report_counter() == 0)
		{
			return nullptr;
		}
		switch (split_direction)
		{
		case cell_split_direction::left_x:
			return split_x(cur_cell->boundary().min.x + 4 * m_ghost_radius, origin_space_id, new_space_game_id, new_space_id, origin_space_id);
		case cell_split_direction::right_x:
			return split_x(cur_cell->boundary().max.x - 4 * m_ghost_radius, origin_space_id, new_space_game_id, origin_space_id, new_space_id);
		case cell_split_direction::low_z:
			return split_z(cur_cell->boundary().min.z + 4 * m_ghost_radius, origin_space_id, new_space_game_id, new_space_id, origin_space_id);
		case cell_split_direction::high_z:
			return split_z(cur_cell->boundary().max.z - 4 * m_ghost_radius, origin_space_id, new_space_game_id, origin_space_id, new_space_id);
		default:
			return nullptr;
		}

	}
}