#include "space_cells.h"

namespace 
{

	template <typename T>
	class vec_iter_wrapper
	{
		bool is_reverse;
		const std::vector<T>& data_vec;
		std::uint64_t idx;
	public:
		vec_iter_wrapper(const std::vector<T>& in_data_vec, bool in_is_reverse)
			: is_reverse(in_is_reverse)
			, data_vec(in_data_vec)
			, idx(0)
		{

		}

		bool valid() const
		{
			return idx < data_vec.size();
		}

		const T& get_and_next()
		{
			auto old_idx = idx;
			idx++;
			if (is_reverse)
			{
				return data_vec[data_vec.size() - old_idx - 1];
			}
			else
			{
				return data_vec[old_idx];
			}
		}
	};
}
namespace spiritsaway::distributed_space
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
	: m_root_node(new space_node(bound, game_id, space_id, nullptr))
	{
		m_leaf_nodes[space_id] = m_root_node;
		m_master_cell_id = space_id;
		m_ghost_radius = in_ghost_radius;
	}

	const space_cells::space_node* space_cells::space_node::sibling() const
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
	void space_cells::space_node::set_ready()
	{
		m_ready = true;
	}

	float space_cells::space_node::get_smoothed_load() const
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
	
	void space_cells::space_node::update_load(float cur_load, const std::vector<entity_load>& new_entity_loads)
	{
		m_cell_load_report_counter++;
		m_cell_loads[m_cell_load_report_counter % m_cell_loads.size()] = cur_load;
		m_entity_loads = new_entity_loads;
		make_sorted_loads();
	}

	void space_cells::space_node::make_sorted_loads()
	{
		m_sorted_entity_load_idx_by_axis[0].resize(m_entity_loads.size(), 0);
		m_sorted_entity_load_idx_by_axis[1].resize(m_entity_loads.size(), 0);
		for (std::uint16_t i = 0; i < m_entity_loads.size(); i++)
		{
			m_sorted_entity_load_idx_by_axis[0][i] = i;
			m_sorted_entity_load_idx_by_axis[1][i] = i;
		}
		std::sort(m_sorted_entity_load_idx_by_axis[0].begin(), m_sorted_entity_load_idx_by_axis[0].end(), [this](std::uint16_t a, std::uint16_t b)
			{
				return this->get_entity_loads()[a].pos.x < this->get_entity_loads()[b].pos.x;
			});
		std::sort(m_sorted_entity_load_idx_by_axis[1].begin(), m_sorted_entity_load_idx_by_axis[1].end(), [this](std::uint16_t a, std::uint16_t b)
			{
				return this->get_entity_loads()[a].pos.z < this->get_entity_loads()[b].pos.z;
			});
	}
	space_cells::space_node* space_cells::space_node::split_x(double x, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id, const std::string& new_parent_space_id)
	{
		if(!is_leaf_cell())
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
		m_children[0] = new space_node(left_boundary, m_game_id, left_space_id, this);
		m_children[1] = new space_node(right_boundary, m_game_id, right_space_id, this);
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

	void space_cells::space_node::on_split(int master_child_index)
	{
		m_children[master_child_index]->m_cell_loads[1] = get_latest_load();
		m_children[master_child_index]->m_cell_load_report_counter = 1;
		m_children[master_child_index]->m_entity_loads = std::move(m_entity_loads);
		m_children[master_child_index]->m_sorted_entity_load_idx_by_axis = std::move(m_sorted_entity_load_idx_by_axis);
		m_children[master_child_index]->set_ready();
	}
	space_cells::space_node* space_cells::space_node::split_z(double z, const std::string& new_space_game_id, const std::string& low_space_id, const std::string& high_space_id, const std::string& new_parent_space_id)
	{
		if(!is_leaf_cell())
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
		m_children[0] = new space_node(low_boundary, m_game_id, low_space_id, this);
		m_children[1] = new space_node(high_boundary, m_game_id, high_space_id, this);
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
	json space_cells::space_node::encode() const
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
		if (!is_leaf_cell())
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
	bool space_cells::space_node::set_child(int index, space_node* new_child)
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

	float space_cells::space_node::get_latest_load() const
	{
		return m_cell_loads[m_cell_load_report_counter % m_cell_loads.size()];
	}
	bool space_cells::space_node::balance(double split_v)
	{
		if(is_leaf_cell())
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
	
	void space_cells::space_node::merge_to_child(const std::string& dest)
	{
		m_entity_loads.clear();
		m_cell_load_report_counter = 1;
		m_space_id = dest;
		if (m_children[0]->space_id() == dest)
		{
			m_game_id = m_children[0]->game_id();
			m_entity_loads = std::move(m_children[0]->m_entity_loads);
			m_sorted_entity_load_idx_by_axis = std::move(m_children[0]->m_sorted_entity_load_idx_by_axis);
			m_cell_loads[1] = m_children[0]->get_latest_load();
		}
		else
		{
			m_game_id = m_children[1]->game_id();
			m_entity_loads = std::move(m_children[1]->m_entity_loads);
			m_sorted_entity_load_idx_by_axis = std::move(m_children[1]->m_sorted_entity_load_idx_by_axis);
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
		auto remove_node_iter = m_leaf_nodes.find(space_id);
		if(remove_node_iter == m_leaf_nodes.end())
		{
			return {};
		}
		auto remove_node = remove_node_iter->second;
		if(!remove_node->is_leaf_cell())
		{
			return {};
		}
		if (!remove_node->ready())
		{
			return {};
		}
		if (remove_node->space_id() == m_master_cell_id)
		{
			return {};
		}
		auto sibling_node = remove_node->sibling();
		if (!sibling_node || !sibling_node->is_leaf_cell())
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
		m_internal_nodes.erase(cur_parent->space_id());
		auto dest_space_id = sibling_node->space_id();
		cur_parent->merge_to_child(dest_space_id);
		
		m_leaf_nodes.erase(remove_node_iter);
		m_leaf_nodes.erase(dest_space_id);
		m_leaf_nodes[dest_space_id] = cur_parent;
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

	const space_cells::space_node* space_cells::split_x(double x, const std::string& origin_space_id, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id)
	{
		if(!check_valid_space_id(left_space_id) || ! check_valid_space_id(right_space_id))
		{
			return nullptr;
		}
		auto dest_node_iter = m_leaf_nodes.find(origin_space_id);
		if(dest_node_iter == m_leaf_nodes.end())
		{
			return nullptr;
		}
		auto dest_node = dest_node_iter->second;
		if(!dest_node->is_leaf_cell())
		{
			return nullptr;
		}
		m_temp_node_counter++;
		auto result = dest_node->split_x(x, new_space_game_id, left_space_id, right_space_id, std::to_string(m_temp_node_counter));
		if(!result)
		{
			return nullptr;
		}
		m_leaf_nodes.erase(dest_node_iter);
		for(auto one_child: dest_node->children())
		{
			m_leaf_nodes[one_child->space_id()] = one_child;
		}
		m_internal_nodes[dest_node->space_id()] = dest_node;
		return result;
		
	}

	const space_cells::space_node* space_cells::split_z(double z, const std::string& origin_space_id, const std::string& new_space_game_id,  const std::string& low_space_id, const std::string& high_space_id)
	{
		if(!check_valid_space_id(low_space_id) || ! check_valid_space_id(high_space_id))
		{
			return nullptr;
		}
		auto dest_node_iter = m_leaf_nodes.find(origin_space_id);
		if(dest_node_iter == m_leaf_nodes.end())
		{
			return nullptr;
		}
		auto dest_node = dest_node_iter->second;
		if(!dest_node->is_leaf_cell())
		{
			return nullptr;
		}
		m_temp_node_counter++;
		auto result = dest_node->split_z(z, new_space_game_id, low_space_id, high_space_id, std::to_string(m_temp_node_counter));
		if(!result)
		{
			return nullptr;
		}
		m_leaf_nodes.erase(dest_node_iter);
		for(auto one_child: dest_node->children())
		{
			m_leaf_nodes[one_child->space_id()] = one_child;
		}
		m_internal_nodes[dest_node->space_id()] = dest_node;
		return result;
		
	}

	std::vector<const space_cells::space_node*> space_cells::query_intersect_leafs(const cell_bound& bound) const
	{
		std::vector<const space_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_node);
		std::vector<const space_cells::space_node*> result;
		while(!temp_query_buffer.empty())
		{
			auto temp_top = temp_query_buffer.back();
			temp_query_buffer.pop_back();
			if(!temp_top->boundary().intersect(bound))
			{
				continue;
			}
			if(temp_top->is_leaf_cell())
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

	const space_cells::space_node* space_cells::query_leaf_for_point(double x, double z) const
	{
		std::vector<const space_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_node);
		while(!temp_query_buffer.empty())
		{
			auto temp_top = temp_query_buffer.back();
			temp_query_buffer.pop_back();
			if(!temp_top->boundary().cover(x, z))
			{
				continue;
			}
			if(temp_top->is_leaf_cell())
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
		std::vector<const space_node*> temp_query_buffer;
		temp_query_buffer.push_back(m_root_node);
		while(!temp_query_buffer.empty())
		{
			auto temp_top = temp_query_buffer.back();
			temp_query_buffer.pop_back();
			cell_jsons.push_back(temp_top->encode());
			if(temp_top->is_leaf_cell())
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
		m_leaf_nodes.clear();
		if (m_root_node)
		{
			delete m_root_node;
			m_root_node = nullptr;
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
				space_node* parent_node = nullptr;

				if(!parent_space_id.empty())
				{
					auto temp_iter = m_leaf_nodes.find(parent_space_id);
					if(temp_iter == m_leaf_nodes.end())
					{
						return false;
					}
					parent_node = temp_iter->second;
				}
				
				auto new_node = new space_node(temp_bound, temp_game_id, temp_space_id, parent_node);
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
				new_node->make_sorted_loads();
				m_leaf_nodes[temp_space_id] = new_node;
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
					m_root_node = new_node;
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
		auto cur_node_iter = m_leaf_nodes.find(cell_id);
		if(cur_node_iter == m_leaf_nodes.end())
		{
			return false;
		}
		auto cur_sibling = cur_node_iter->second->sibling();
		if (!cur_sibling || !cur_sibling->is_leaf_cell())
		{
			return false;
		}
		cur_node_iter->second->m_parent->balance(split_v);
		return true;
		
	}
	std::vector<std::string> space_cells::all_child_space_except(const std::string& except_space) const
	{
		std::vector<std::string> result;
		result.reserve(m_leaf_nodes.size() /2 + 2);
		for(const auto& one_cell: m_leaf_nodes)
		{
			if(!one_cell.second->is_leaf_cell())
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
		auto cur_node_iter = m_leaf_nodes.find(cell_id);
		if(cur_node_iter == m_leaf_nodes.end())
		{
			return false;
		}
		cur_node_iter->second->set_ready();
		return true;
	}

	space_cells::~space_cells()
	{
		for(auto one_pair: m_leaf_nodes)
		{
			delete one_pair.second;
		}
		m_leaf_nodes.clear();
		m_root_node = nullptr;
	}

	void space_cells::update_cell_load(const std::string& cell_space_id, float cell_load, const std::vector<entity_load>& new_entity_loads)
	{
		auto cur_node_iter = m_leaf_nodes.find(cell_space_id);
		if(cur_node_iter == m_leaf_nodes.end())
		{
			return;
		}
		if (cur_node_iter->second->is_leaf_cell())
		{
			cur_node_iter->second->update_load(cell_load, new_entity_loads);
		}

	}

	bool space_cells::space_node::calc_offset_axis(float load_to_offset,  double& out_split_axis, float& offseted_load, float ghost_radius) const
	{
		if (!is_leaf_cell())
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
		double split_boundary = 0;
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
		auto temp_sorted_indexes = vec_iter_wrapper(m_sorted_entity_load_idx_by_axis[is_x ? 0 : 1], should_reverse);
		
		float accumulated_load = 0;
		double pre_split_candidate = std::min(m_boundary.min.x, m_boundary.min.z) - 100;
		
		while(temp_sorted_indexes.valid())
		{
			auto one_entity_load_idx = temp_sorted_indexes.get_and_next();
			const auto& one_entity_load = m_entity_loads[one_entity_load_idx];

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

	const space_cells::space_node* space_cells::get_best_cell_to_split(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param) const
	{
		const space_cells::space_node* best_result = nullptr;
		for (const auto& [one_cell_id, one_cell_node] : m_leaf_nodes)
		{
			if (!one_cell_node->is_leaf_cell())
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

	const space_cells::space_node* space_cells::get_best_cell_to_remove(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param)
	{
		const space_cells::space_node* best_result = nullptr;
		for (const auto& [one_cell_id, one_cell_node] : m_leaf_nodes)
		{
			if (!one_cell_node->is_leaf_cell())
			{
				continue;
			}
			if (one_cell_node->space_id() == m_master_cell_id)
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

	bool space_cells::space_node::check_can_shrink(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param, const double ghost_radius) const
	{
		if (!m_parent)
		{
			return false;
		}
		if (m_min_cell_load_report_counter <= lb_param.min_cell_load_report_counter_when_shrink)
		{
			return false;
		}
		auto cur_sibling = sibling();
		if (!cur_sibling || cur_sibling->m_min_cell_load_report_counter <= lb_param.min_cell_load_report_counter_when_shrink)
		{
			return false;
		}
		auto avg_game_load = m_total_game_load / m_child_games.size();
		if (avg_game_load < lb_param.min_cell_load_when_shrink)
		{
			return false;
		}
		auto sibling_game_load = cur_sibling->m_total_game_load / cur_sibling->m_child_games.size();
		if (avg_game_load - sibling_game_load < lb_param.min_sibling_game_load_diff_when_shrink)
		{
			return false;
		}

		// 缩容时最小步长为 ghost_radius 同时由于要保证每个cell的边长要大于4*ghost_radius 所以这里需要大于5倍的ghost_radius
		if (calc_max_boundary_move_length(m_parent->is_split_x(), m_parent->m_children[0] == this) < 5* ghost_radius)
		{
			return false;
		}
		return true;

	}

	const space_cells::space_node* space_cells::space_node::calc_shrink_node(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param, const double ghost_radius) const
	{
		for (auto one_child : m_children)
		{
			if (one_child)
			{
				auto cur_child_result = one_child->calc_shrink_node(game_loads, lb_param, ghost_radius);
				if (cur_child_result)
				{
					return cur_child_result;
				}
			}
		}
		if (check_can_shrink(game_loads, lb_param, ghost_radius))
		{
			return this;
		}
		return nullptr;
	}

	const space_cells::space_node* space_cells::get_best_node_to_shrink(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param)
	{
		return m_root_node->calc_shrink_node(game_loads, lb_param, m_ghost_radius);
	}

	cell_split_direction space_cells::space_node::calc_best_split_direction(float ghost_radius) const
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
			
			float temp_acc_loads = 0;
			for (int i = 0; i < 1; i++)
			{
				auto cur_sorted_load_idx_copy = vec_iter_wrapper(m_sorted_entity_load_idx_by_axis[i], false);

				while (cur_sorted_load_idx_copy.valid())
				{
					auto one_index = cur_sorted_load_idx_copy.get_and_next();
					const auto& one_load = m_entity_loads[one_index];
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
				
				temp_acc_loads = 0;
				auto cur_sorted_load_idx_copy_reverse = vec_iter_wrapper(m_sorted_entity_load_idx_by_axis[i], true);

				while (cur_sorted_load_idx_copy_reverse.valid())
				{
					auto one_index = cur_sorted_load_idx_copy_reverse.get_and_next();
					const auto& one_load = m_entity_loads[one_index];
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
			// 避免新的子节点与原来的兄弟节点划分方向相同 以免出现连续多个同方向划分
			if (m_parent)
			{
				if (m_parent->is_split_x())
				{
					if (this == m_parent->m_children[0])
					{
						split_gains[int(cell_split_direction::right_x)] *= 0.5f;
					}
					else
					{
						split_gains[int(cell_split_direction::left_x)] *= 0.5f;
					}
				}
				else
				{
					if (this == m_parent->m_children[0])
					{
						split_gains[int(cell_split_direction::high_z)] *= 0.5f;
					}
					else
					{
						split_gains[int(cell_split_direction::low_z)] *= 0.5f;
					}
				}
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

	const space_cells::space_node* space_cells::split_at_direction(const std::string& origin_space_id, cell_split_direction split_direction, const std::string& new_space_id, const std::string& new_space_game_id)
	{
		auto cur_cell_iter = m_leaf_nodes.find(origin_space_id);
		if (cur_cell_iter == m_leaf_nodes.end())
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

	double space_cells::space_node::calc_max_boundary_move_length(bool is_x, bool is_split_pos_smaller) const
	{
		auto cur_axis = is_x ? 0 : 1;
		if (is_leaf_cell())
		{
			return m_boundary.max[is_x] - m_boundary.min[is_x];
		}
		else
		{
			if (is_x)
			{
				if (m_is_split_x)
				{
					if (is_split_pos_smaller)
					{
						return m_children[1]->calc_max_boundary_move_length(is_x, is_split_pos_smaller);
					}
					else
					{
						return m_children[0]->calc_max_boundary_move_length(is_x, is_split_pos_smaller);
					}
					
				}
				else
				{
					return std::min(m_children[0]->calc_max_boundary_move_length(is_x, is_split_pos_smaller), m_children[1]->calc_max_boundary_move_length(is_x, is_split_pos_smaller));
				}
			}
			else
			{
				if (m_is_split_x)
				{
					return std::min(m_children[0]->calc_max_boundary_move_length(is_x, is_split_pos_smaller), m_children[1]->calc_max_boundary_move_length(is_x, is_split_pos_smaller));
					

				}
				else
				{
					if (is_split_pos_smaller)
					{
						return m_children[1]->calc_max_boundary_move_length(is_x, is_split_pos_smaller);
					}
					else
					{
						return m_children[0]->calc_max_boundary_move_length(is_x, is_split_pos_smaller);
					}
				}
			}
			
		}
		
	}
	void space_cells::space_node::update_boundary_with_new_split(double new_split_pos, bool is_x, bool is_split_pos_smaller, bool is_changing_max)
	{
		auto cur_axis = is_x ? 0 : 1;
		if (is_changing_max)
		{
			m_boundary.max[cur_axis] = new_split_pos;
		}
		else
		{
			m_boundary.min[cur_axis] = new_split_pos;
		}
		if (is_leaf_cell())
		{
			
			m_cell_loads[1] = get_latest_load();
			m_cell_load_report_counter = 1;
		}
		else
		{
			if (is_x)
			{
				if (m_is_split_x)
				{
					if (is_changing_max)
					{
						return m_children[1]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
					}
					else
					{
						return m_children[0]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
					}

				}
				else
				{
					m_children[1]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
					m_children[0]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
				}
			}
			else
			{
				if (m_is_split_x)
				{
					m_children[1]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
					m_children[0]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
				}
				else
				{
					if (is_changing_max)
					{
						return m_children[1]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
					}
					else
					{
						return m_children[0]->update_boundary_with_new_split(new_split_pos, is_x, is_split_pos_smaller, is_changing_max);
					}
				}
			}

		}
	}
	double space_cells::calc_max_shrink_length(const space_node* shrink_node) const
	{
		auto cur_parent = shrink_node->parent();
		if (!cur_parent)
		{
			return -4*m_ghost_radius;
		}
		else
		{
			if (cur_parent->is_split_x())
			{
				if (shrink_node == cur_parent->children()[0])
				{
					return shrink_node->calc_max_boundary_move_length(true, false) - 4 * m_ghost_radius;
				}
				else
				{
					return shrink_node->calc_max_boundary_move_length(true, true) - 4 * m_ghost_radius;
				}
			}
			else
			{
				if (shrink_node == cur_parent->children()[0])
				{
					return shrink_node->calc_max_boundary_move_length(false, false) - 4 * m_ghost_radius;
				}
				else
				{
					return shrink_node->calc_max_boundary_move_length(false, true) - 4 * m_ghost_radius;
				}
			}
		}
	}

	bool space_cells::balance(double split_v, const space_cells::space_node* cur_node)
	{
		if (!cur_node || cur_node->is_leaf_cell())
		{
			return false;
		}
		double pre_split_pos = cur_node->m_children[0]->boundary().max.x;
		if (!cur_node->is_split_x())
		{
			pre_split_pos = cur_node->m_children[0]->boundary().max.z;
		}
		auto mutable_cur_node = m_internal_nodes[cur_node->space_id()];
		assert(mutable_cur_node);
		mutable_cur_node->children()[0]->update_boundary_with_new_split(split_v, cur_node->is_split_x(), split_v < pre_split_pos, true);
		mutable_cur_node->children()[1]->update_boundary_with_new_split(split_v, cur_node->is_split_x(), split_v < pre_split_pos, false);
		return true;
	}

	float space_cells::space_node::calc_move_split_offload(double new_split_pos, bool is_x, bool is_split_pos_smaller) const
	{
		auto cur_axis = is_x ? 0 : 1;
		if (is_leaf_cell())
		{
			auto cur_sorted_load_idx_copy = vec_iter_wrapper(m_sorted_entity_load_idx_by_axis[cur_axis], is_split_pos_smaller);
			
			float temp_gain = 0;
			while (cur_sorted_load_idx_copy.valid())
			{
				auto one_index = cur_sorted_load_idx_copy.get_and_next();
				if (is_split_pos_smaller)
				{
					if (m_entity_loads[one_index].pos[cur_axis] < new_split_pos)
					{
						break;
					}
				}
				else
				{
					if (m_entity_loads[one_index].pos[cur_axis] > new_split_pos)
					{
						break;
					}
				}
				temp_gain += m_entity_loads[one_index].load;
			}
			return temp_gain;
		}
		else
		{
			if (is_x)
			{
				if (m_is_split_x)
				{
					if (is_split_pos_smaller)
					{
						return m_children[1]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller);
					}
					else
					{
						return m_children[0]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller);
					}

				}
				else
				{
					return m_children[1]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller) + m_children[0]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller);
				}
			}
			else
			{
				if (m_is_split_x)
				{
					return m_children[1]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller) + m_children[0]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller);
				}
				else
				{
					if (is_split_pos_smaller)
					{
						return m_children[0]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller);
					}
					else
					{
						return m_children[1]->calc_move_split_offload(new_split_pos, is_x, is_split_pos_smaller);
					}
				}
			}

		}
	}

	void space_cells::space_node::update_load_stat(const std::unordered_map<std::string, float>& game_loads)
	{
		if (is_leaf_cell())
		{
			m_min_cell_load_report_counter = m_cell_load_report_counter;
			m_total_cell_load = get_smoothed_load();
			auto temp_game_iter = game_loads.find(m_game_id);
			if (temp_game_iter == game_loads.end())
			{
				m_total_game_load = 0;
			}
			else
			{
				m_total_game_load = temp_game_iter->second;
			}
			m_child_games.clear();
			m_child_leaf.clear();
			m_child_games.push_back(m_game_id);
			m_child_leaf.push_back(this);
			
		}
		else
		{
			m_children[0]->update_load_stat(game_loads);
			m_children[1]->update_load_stat(game_loads);
			m_total_cell_load = m_children[0]->m_total_cell_load + m_children[1]->m_total_cell_load;
			m_total_game_load = m_children[0]->m_total_game_load + m_children[1]->m_total_game_load;
			m_child_games.clear();
			m_child_leaf.clear();
			m_min_cell_load_report_counter = std::min(m_children[0]->m_min_cell_load_report_counter, m_children[1]->m_min_cell_load_report_counter);
			for (const auto& one_node : m_children[0]->m_child_leaf)
			{
				m_child_leaf.push_back(one_node);
			}
			for (const auto& one_game : m_children[0]->m_child_games)
			{
				m_child_games.push_back(one_game);
			}
			for (const auto& one_node : m_children[1]->m_child_leaf)
			{
				m_child_leaf.push_back(one_node);
			}
			for (const auto& one_game : m_children[1]->m_child_games)
			{
				m_child_games.push_back(one_game);
			}
		}
	}

	double space_cells::space_node::calc_best_shrink_new_split_pos(const cell_load_balance_param& lb_param, const double ghost_radius) const
	{
		bool is_split_pos_smaller = m_parent->m_children[0] == this;
		auto max_move_length = calc_max_boundary_move_length(m_parent->is_split_x(), is_split_pos_smaller);
		max_move_length -= 5 * ghost_radius;
		auto move_unit = max_move_length / 10;
		auto cur_split_pos = m_boundary.min.x;
		for (int i = 0; i < 10; i++)
		{
			if (m_parent->is_split_x())
			{
				if (is_split_pos_smaller)
				{
					cur_split_pos = m_boundary.max.x - ghost_radius - i * move_unit;
				}
				else
				{
					cur_split_pos = m_boundary.min.x + ghost_radius + i * move_unit;
				}
			}
			else
			{
				if (is_split_pos_smaller)
				{
					cur_split_pos = m_boundary.max.z - ghost_radius - i * move_unit;
				}
				else
				{
					cur_split_pos = m_boundary.min.z + ghost_radius + i * move_unit;
				}
			}
			auto cur_split_offload = calc_move_split_offload(cur_split_pos, m_parent->is_split_x(), is_split_pos_smaller);
			if (cur_split_offload > lb_param.min_sibling_game_load_diff_when_shrink / 2)
			{
				return cur_split_pos;
			}
		}
		return cur_split_pos;
	}

	void space_cells::update_load_stat(const std::unordered_map<std::string, float>& game_loads)
	{
		m_root_node->update_load_stat(game_loads);
	}
}