#pragma once
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
namespace spiritsaway::utility
{

	struct entity_load
	{
		std::array<double, 2> pos; // (x,z)
		double load;
		std::string name;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(entity_load, pos, load, name)
	};

	class cell_region
	{
	public:
		struct cell_bound
		{
			double left_x;
			double low_z;
			double right_x;
			double high_z;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(cell_bound, left_x, low_z, right_x, high_z)

			bool cover(const double x, const double z) const
			{
				if(left_x > x || right_x < x)
				{
					return false;
				}
				if(low_z > z || high_z < z)
				{
					return false;
				}
				return true;
			}
			bool intersect(const cell_bound& other) const;
		};
	public:
		class cell_node
		{
		private:
			std::string m_space_id;
			std::string m_game_id;
			cell_bound m_boundary;
			std::array<cell_node*, 2> m_children;
			cell_node* m_parent = nullptr;
			bool m_ready = false;
			bool m_is_split_x = false;
			std::array<double, 4> m_cell_loads;
			std::vector<entity_load> m_entity_loads;
			std::uint32_t m_cell_load_counter = 0;
			
		public:
			cell_node(const cell_bound& in_bound, const std::string& in_game_id, const std::string& in_space_id, cell_node* in_parent)
			: m_space_id(in_space_id)
			, m_game_id(in_game_id)
			, m_boundary(in_bound)
			, m_children{nullptr, nullptr}
			, m_parent(in_parent)
			{

			}
			cell_node* split_x(double x, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id, const std::string& new_parent_space_id);
			cell_node* split_z(double z, const std::string& new_space_game_id, const std::string& low_space_id, const std::string& up_space_id, const std::string& new_parent_space_id);
			bool balance(bool is_x, double split_v);
			bool is_leaf() const
			{
				return !m_children[0];
			}
			bool ready() const
			{
				return m_ready;
			}

			const std::string& space_id() const
			{
				return m_space_id;
			}

			const std::string& game_id() const
			{
				return m_game_id;
			}
			const cell_bound& boundary() const
			{
				return m_boundary;
			}
			const std::array<cell_node*, 2>& children() const
			{
				return m_children;
			}
			const cell_node* sibling() const;

			cell_node* parent()
			{
				return m_parent;
			}
			bool can_merge_to_child(const std::string& dest, const std::string& master_cell_id) const;
			std::pair<std::string, std::string> merge_to_child(const std::string& dest, const std::string& master_cell_id);

			json encode() const;
			double get_smoothed_load() const;
			void add_load(double cur_load, std::vector<entity_load>& new_entity_loads);
			// 计算如果需要减少load_to_offset的负载，应该切分的位置
			bool calc_offset_axis(double load_to_offset, double& out_split_axis, double& offseted_load) const;
		private:
			bool set_child(int index, cell_node* new_child);
			void set_ready();
			void on_split(int master_child_index);
			friend class cell_region;
			
		};
		std::unordered_map<std::string, cell_node*> m_cells;
		cell_node* m_root_cell;
		std::uint64_t m_temp_node_counter = 0;
		// master 代表主逻辑cell 这个cell的生命周期伴随着整个space的生命周期
		// space的主体逻辑由master cell控制
		// 第一个创建的cell就是master_cell
		std::string m_master_cell_id;
	public:
		
		cell_region(const cell_bound& bound, const std::string& game_id, const std::string& space_id);
		const cell_node* split_x(double x, const std::string& origin_space_id, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id);
		const cell_node* split_z(double z, const std::string& origin_space_id, const std::string& new_space_game_id,const std::string& low_space_id, const std::string& high_space_id);
		bool balance(bool is_x, double split_v, const std::string& cell_id);
		std::pair<std::string, std::string> merge_to(const std::string& space_id);
		std::vector<const cell_node*> query_intersect(const cell_bound& bound) const;
		const cell_node* query_point_region(double x, double z) const;
		const std::unordered_map<std::string, cell_node*>& cells() const
		{
			return m_cells;
		}
		const cell_node* get_cell(const std::string& cell_id) const
		{
			auto cur_iter = m_cells.find(cell_id);
			if(cur_iter == m_cells.end())
			{
				return nullptr;
			}
			else
			{
				return cur_iter->second;
			}
		}
		const cell_node* root_cell() const
		{
			return m_root_cell;
		}
		bool set_ready(const std::string& space_id);
		json encode() const;
		bool decode(const json& data);
		bool check_valid_space_id(const std::string& cell_id) const;
		std::vector<std::string> all_child_space_except(const std::string& except_space) const;
		const std::string& master_cell_id() const
		{
			return m_master_cell_id;
		}
		void add_load(const std::string& cell_space_id, double cell_load);
		~cell_region();
	};
}