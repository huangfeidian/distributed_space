#pragma once
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
namespace spiritsaway::utility
{
	struct point_xz
	{
		union
		{
			struct {
				double x;
				double z;
			};
			double val[2];
		};
		
		double& operator[](int index)
		{
			return val[index];
		}
		double operator[](int index) const
		{
			return val[index];
		}
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(point_xz, x, z)
	};
	struct entity_load
	{
		point_xz pos; // (x,z)
		float load;
		bool is_real;
		std::string name;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(entity_load, pos, load, name, is_real)
	};


	struct cell_bound
	{
		point_xz min;
		point_xz max;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(cell_bound, min, max);

		bool cover(const double x, const double z) const
		{
			if (min.x > x || max.x < x)
			{
				return false;
			}
			if (min.z > z || max.z < z)
			{
				return false;
			}
			return true;
		}
		bool intersect(const cell_bound& other) const;
	};
	class space_cells
	{
	public:
		
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
			std::array<float, 4> m_cell_loads;
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
				std::fill(m_cell_loads.begin(), m_cell_loads.end(), 0);
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
			const auto& get_entity_loads() const
			{
				return m_entity_loads;
			}

			bool is_split_x() const
			{
				return m_is_split_x;
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
			float get_smoothed_load() const;
			std::uint32_t cell_load_counter() const
			{
				return m_cell_load_counter;
			}
			void update_load(float cur_load, const std::vector<entity_load>& new_entity_loads);
			// 计算如果需要减少load_to_offset的负载，应该切分的位置
			bool calc_offset_axis(float load_to_offset, double& out_split_axis, float& offseted_load) const;
		private:
			bool set_child(int index, cell_node* new_child);
			void set_ready();
			void on_split(int master_child_index);
			friend class space_cells;
			
		};
	private:
		std::unordered_map<std::string, cell_node*> m_cells;
		cell_node* m_root_cell;
		std::uint64_t m_temp_node_counter = 0;
		// master 代表主逻辑cell 这个cell的生命周期伴随着整个space的生命周期
		// space的主体逻辑由master cell控制
		// 第一个创建的cell就是master_cell
		std::string m_master_cell_id;
		double m_ghost_radius;
	public:
		
		space_cells(const cell_bound& bound, const std::string& game_id, const std::string& space_id, const double in_ghost_radius);
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
		const auto& all_cells() const
		{
			return m_cells;
		}
		auto ghost_radius() const
		{
			return m_ghost_radius;
		}
		bool set_ready(const std::string& space_id);
		json encode() const;
		bool decode(const json& data);
		// 传入的cell id不能是数字
		bool check_valid_space_id(const std::string& cell_id) const;
		std::vector<std::string> all_child_space_except(const std::string& except_space) const;
		const std::string& master_cell_id() const
		{
			return m_master_cell_id;
		}
		void update_cell_load(const std::string& cell_space_id, float cell_load, const std::vector<entity_load>& new_entity_loads);

		// 选择一个合适的cell来分割 分割要求
		// 1. 这个cell所在的game load 要大于指定阈值
		// 2. 这个cell的load要大于指定阈值
		// 3. 选取cell load最大的
		// 4. 避免选取刚刚创建的cell
		const cell_node* get_best_cell_to_split(const std::unordered_map<std::string, float>& game_loads, const float min_game_load, const float min_cell_load) const;
		~space_cells();

		// 选择一个合适的cell来删除 删除要求
		// 1. 这个cell的负载要小于指定阈值 max_cell_load
		// 2. 这个cell的兄弟节点的game负载要比此cell的game负载低起码min_sibling_game_load_diff
		// 3. 选取其中 cell load最小的
		// 4. 避免选取刚刚创建的cell
		const cell_node* get_best_cell_to_remove(const std::unordered_map<std::string, float>& game_loads, const float min_sibling_game_load_diff, const float max_cell_load);

		// 选择一个合适的cell来缩小边界 缩容要求
		// 1. 这个cell的负载要比其兄弟负载高起码 min_cell_load_diff
		// 2. 这个cell的兄弟节点的game负载要比此cell的game负载高起码min_sibling_game_load_diff
		// 3. 选取其中 cell load最大的
		// 4. 避免选取刚刚创建的cell 或者其兄弟节点刚刚创建
		const cell_node* get_best_cell_to_shrink(const std::unordered_map<std::string, float>& game_loads, const float min_sibling_game_load_diff, const float min_cell_load_diff);
	};
}