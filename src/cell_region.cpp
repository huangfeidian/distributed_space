#include "cell_region.h"

namespace spiritsaway::utility
{
	bool cell_region::cell_bound::intersect(const cell_region::cell_bound& other) const
	{
		if(left_x >= other.right_x)
		{
			return false;
		}
		if(right_x <= other.left_x)
		{
			return false;
		}
		if(low_z >= other.high_z)
		{
			return false;
		}
		if(high_z <= other.low_z)
		{
			return false;
		}
		return true;
	}
	cell_region::cell_region(const cell_bound& bound, const std::string& game_id, const std::string& space_id)
	: m_root_cell(new cell_node(bound, game_id, space_id, nullptr))
	{
		m_cells[space_id] = m_root_cell;
		m_master_cell_id = space_id;
	}

	const cell_region::cell_node* cell_region::cell_node::sibling() const
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
	void cell_region::cell_node::set_ready()
	{
		m_ready = true;
	}

	double cell_region::cell_node::get_smoothed_load() const
	{
		double square_sum = 0;
		double total_weights = 0.01;
		double current_weight = 1.0;
		std::uint32_t back_iter_idx = m_cell_load_counter;
		while(back_iter_idx > 0 && m_cell_load_counter - back_iter_idx < m_cell_loads.size() )
		{
			auto cur_period_load = m_cell_loads[back_iter_idx % m_cell_loads.size()];
			if(cur_period_load == 0)
			{
				break;
			}
			square_sum += cur_period_load * cur_period_load * current_weight;
			total_weights += current_weight;
			back_iter_idx--;
			current_weight -= 0.2;
		}
		
		return std::sqrt(square_sum/total_weights);
	}
	
	void cell_region::cell_node::add_load(double cur_load, std::vector<entity_load>& new_entity_loads)
	{
		m_cell_load_counter++;
		m_cell_loads[m_cell_load_counter % m_cell_loads.size()] = cur_load;
		m_entity_loads = std::move(new_entity_loads);
	}

	cell_region::cell_node* cell_region::cell_node::split_x(double x, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id, const std::string& new_parent_space_id)
	{
		if(!is_leaf())
		{
			return nullptr;
		}
		if(x <= m_boundary.left_x || x >= m_boundary.right_x)
		{
			return nullptr;
		}
		if(m_space_id != left_space_id && m_space_id != right_space_id)
		{
			return nullptr;
		}
		cell_bound left_boundary, right_boundary;
		left_boundary = right_boundary = m_boundary;
		left_boundary.right_x = x;
		right_boundary.left_x = x;
		m_is_split_x = true;
		m_children[0] = new cell_node(left_boundary, m_game_id, left_space_id, this);
		m_children[1] = new cell_node(right_boundary, m_game_id, right_space_id, this);
		auto pre_space_id = m_space_id;
		m_space_id = new_parent_space_id;
		int master_cell_idx = 0;
		if (pre_space_id != left_space_id)
		{
			master_cell_idx = 1;
		}
		on_split(master_cell_idx);
		m_children[1 - master_cell_idx]->m_game_id = new_space_game_id;
		return m_children[1 - master_cell_idx];
	}

	void cell_region::cell_node::on_split(int master_child_index)
	{
		m_children[master_child_index]->m_cell_loads = std::move(m_cell_loads);
		m_children[master_child_index]->m_cell_load_counter = std::move(m_cell_load_counter);
		m_children[master_child_index]->m_entity_loads = std::move(m_entity_loads);
		m_children[master_child_index]->set_ready();
	}
	cell_region::cell_node* cell_region::cell_node::split_z(double z, const std::string& new_space_game_id, const std::string& low_space_id, const std::string& high_space_id, const std::string& new_parent_space_id)
	{
		if(!is_leaf())
		{
			return nullptr;
		}
		if(z <= m_boundary.low_z || z >= m_boundary.high_z)
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
		low_boundary.high_z = z;
		high_boundary.low_z = z;
		m_children[0] = new cell_node(low_boundary, m_game_id, low_space_id, this);
		m_children[1] = new cell_node(high_boundary, m_game_id, high_space_id, this);
		auto pre_space_id = m_space_id;
		m_space_id = new_parent_space_id;
		int master_cell_idx = 0;
		if (pre_space_id != low_space_id)
		{
			master_cell_idx = 1;
		}
		on_split(master_cell_idx);
		m_children[1 - master_cell_idx]->m_game_id = new_space_game_id;
		return m_children[1 - master_cell_idx];
	}
	json cell_region::cell_node::encode() const
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
			result["cell_load_counter"] = m_cell_load_counter;
		}
		
		return result;
	}
	bool cell_region::cell_node::set_child(int index, cell_node* new_child)
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
	bool cell_region::cell_node::can_merge_to_child(const std::string& dest, const std::string& master_cell_id) const
	{
		if(!m_children[0] || !m_children[1])
		{
			return false;
		}
		if(!m_children[0]->is_leaf() || !m_children[1]->is_leaf())
		{
			return false;
		}
		if(!m_children[0]->ready() || !m_children[1]->ready())
		{
			return false;
		}
		if(dest == m_children[0]->m_space_id)
		{
			if(m_children[1]->m_space_id == master_cell_id)
			{
				return false;
			}
			else
			{
				return true;
			}
		}
		else if(dest == m_children[1]->m_space_id)
		{
			if(m_children[0]->m_space_id == master_cell_id)
			{
				return false;
			}
			else
			{
				return true;
			}
		}
		else
		{
			return false;
		}
	}

	bool cell_region::cell_node::balance(bool is_x, double split_v)
	{
		if(is_leaf())
		{
			return false;
		}
		if(is_x)
		{
			if(m_children[0]->m_boundary.left_x == m_children[1]->m_boundary.left_x)
			{
				return false;
			}
			if(split_v <= m_children[0]->m_boundary.left_x || split_v >= m_children[1]->m_boundary.right_x)
			{
				return false;
			}
			m_children[0]->m_boundary.right_x = split_v;
			m_children[1]->m_boundary.left_x = split_v;
		}
		else
		{
			if(m_children[0]->m_boundary.low_z == m_children[1]->m_boundary.low_z)
			{
				return false;
			}
			if(split_v <= m_children[0]->m_boundary.low_z || split_v >= m_children[1]->m_boundary.high_z)
			{
				return false;
			}
			m_children[0]->m_boundary.high_z = split_v;
			m_children[1]->m_boundary.low_z = split_v;
		}
		return true;
	}
	// (game_id, space_id)
	std::pair<std::string, std::string> cell_region::cell_node::merge_to_child(const std::string& dest, const std::string& master_cell_id)
	{
		if(!can_merge_to_child(dest, master_cell_id))
		{
			return {};
		}
		std::pair<std::string, std::string> child_to_del;
		cell_node* master_cell = m_children[0];
		if(dest == m_children[0]->m_space_id)
		{
			child_to_del.first = m_children[1]->m_game_id;
			child_to_del.second = m_children[1]->m_space_id;
		}
		else
		{
			master_cell = m_children[1];
			child_to_del.first = m_children[0]->m_game_id;
			child_to_del.second = m_children[0]->m_space_id;
		}
		m_cell_loads = std::move(master_cell->m_cell_loads);
		m_entity_loads = std::move(master_cell->m_entity_loads);
		m_cell_load_counter = std::move(master_cell->m_cell_load_counter);
		m_space_id = dest;
		m_ready = true;
		delete m_children[0];
		delete m_children[1];
		m_children[0] = nullptr;
		m_children[1] = nullptr;
		return child_to_del;
	}

	// (game_id, space_id)
	std::pair<std::string, std::string> cell_region::merge_to(const std::string& space_id)
	{
		auto dest_node_iter = m_cells.find(space_id);
		if(dest_node_iter == m_cells.end())
		{
			return {};
		}
		auto dest_node = dest_node_iter->second;
		if(!dest_node->is_leaf())
		{
			return {};
		}
		auto cur_parent = dest_node->parent();
		if(!cur_parent)
		{
			return {};
		}
		auto child_to_del = cur_parent->merge_to_child(space_id, m_master_cell_id);
		if(child_to_del.second.empty())
		{
			return {};
		}
		m_cells.erase(dest_node_iter);
		m_cells.erase(child_to_del.second);
		m_cells[space_id] = cur_parent;
		return child_to_del;
	}
	bool cell_region::check_valid_space_id(const std::string& space_id) const
	{
		std::uint64_t temp_id;
		try
		{
			temp_id = std::stoull(space_id);
			(void)temp_id;
			return false;
		}
		catch(const std::exception& e)
		{
			(void)temp_id;
			(void)e;
			return true;
		}
		
	}
	const cell_region::cell_node* cell_region::split_x(double x, const std::string& origin_space_id, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id)
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

	const cell_region::cell_node* cell_region::split_z(double z, const std::string& new_space_game_id, const std::string& origin_space_id, const std::string& low_space_id, const std::string& high_space_id)
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

	std::vector<const cell_region::cell_node*> cell_region::query_intersect(const cell_bound& bound) const
	{
		std::vector<const cell_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_cell);
		std::vector<const cell_region::cell_node*> result;
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

	const cell_region::cell_node* cell_region::query_point_region(double x, double z) const
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

	json cell_region::encode() const
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
		return result;
	}

	bool cell_region::decode(const json& data)
	{
		std::string parent_space_id;
		std::array<std::string, 2> children_ids;
		cell_bound temp_bound;
		std::string temp_space_id;
		std::string temp_game_id;
		std::unordered_map<std::string, int> children_indexes;
		bool temp_ready;
		json::array_t cell_jsons;
		try
		{
			data.at("cells").get_to(cell_jsons);
			data.at("master_cell_id").get_to(m_master_cell_id);
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
				if (children_ids.empty())
				{
					one_node.at("cell_loads").get_to(new_node->m_cell_loads);
					one_node.at("entity_loads").get_to(new_node->m_entity_loads);
					one_node.at("cell_load_counter").get_to(new_node->m_cell_load_counter);
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
	bool cell_region::balance(bool is_x, double split_v, const std::string& cell_id)
	{
		auto cur_node_iter = m_cells.find(cell_id);
		if(cur_node_iter == m_cells.end())
		{
			return false;
		}
		cur_node_iter->second->balance(is_x, split_v);
		return true;
		
	}
	std::vector<std::string> cell_region::all_child_space_except(const std::string& except_space) const
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

	bool cell_region::set_ready(const std::string& cell_id)
	{
		auto cur_node_iter = m_cells.find(cell_id);
		if(cur_node_iter == m_cells.end())
		{
			return false;
		}
		cur_node_iter->second->set_ready();
		return true;
	}

	cell_region::~cell_region()
	{
		for(auto one_pair: m_cells)
		{
			delete one_pair.second;
		}
		m_cells.clear();
		m_root_cell = nullptr;
	}

	void cell_region::add_load(const std::string& cell_space_id, double cell_load)
	{
		auto cur_node_iter = m_cells.find(cell_space_id);
		if(cur_node_iter == m_cells.end())
		{
			return;
		}

	}

	bool cell_region::cell_node::calc_offset_axis(double load_to_offset,  double& out_split_axis, double& offseted_load) const
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
		if (is_x)
		{
			if (m_boundary.left_x < cur_sibling->boundary().left_x)
			{
				should_reverse = true;
			}
		}
		else
		{
			axis = 1;
			if (m_boundary.low_z < cur_sibling->boundary().low_z)
			{
				should_reverse = true;
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
		
		double accumulated_load = 0;
		double pre_split_candidate = sorted_entity_loads[0].pos[axis];
		for (const auto& one_entity_load : sorted_entity_loads)
		{
			if (one_entity_load.pos[axis] != pre_split_candidate)
			{
				if (accumulated_load >= load_to_offset)
				{
					offseted_load = accumulated_load;
					out_split_axis = pre_split_candidate;
					return true;
				}
				pre_split_candidate = one_entity_load.pos[axis];
			}
			accumulated_load += one_entity_load.load;
		}
		return false;
	}
}