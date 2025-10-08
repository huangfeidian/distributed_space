#pragma once
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
namespace spiritsaway::distributed_space
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

	enum class cell_load_balance_operation
	{
		nothing,
		remove,
		split,
		shrink,
	};

	enum class cell_split_direction
	{
		left_x, // 从左侧x切分出 4*ghost_radius的范围
		right_x, // 从右侧x切分出 4*ghost_radius的范围
		low_z, // 从下方z切分出 4*ghost_radius的范围
		high_z, // 从上方的z切分出 4*ghost_radius的范围
	};

	// 负载均衡相关参数
	// 一般来说 shrink的report_counter周期最小
	// split的周期要比shrink大一倍
	// remove的周期要比split大一倍
	// 执行负载均衡时 优先执行split操作 因为这样平摊负载最快，
	// 然后是 shrink操作 最后的remove操作相当于gc
	struct cell_load_balance_param
	{
		float max_cell_load_when_remove; // 在考虑remove时 一个cell的最大负载
		std::uint32_t min_cell_load_report_counter_when_remove; // 在考虑remove时 一个cell的最小负载汇报次数

		float min_cell_load_when_shrink; // 在shrink时 当前cell的load 的最小阈值
		std::uint32_t min_cell_load_report_counter_when_shrink; // 在考虑shrink时 一个cell的最小负载汇报次数

		float min_sibling_game_load_diff_when_shrink; // 在缩容时 当前节点的game平均负载起码要比邻居节点的平均game负载高这个值
		float min_game_load_when_split; // 在split时 当前cell所在game的最小负载
		float min_cell_load_when_split; // 在split时 当前cell的最小load
		std::uint32_t min_cell_load_report_counter_when_split; // 在考虑split时 一个cell的最小负载汇报次数
		
		float load_to_offset; // 在考虑shrink的时候 每次缩小的load
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
		class space_node
		{
		private:
			std::string m_space_id;
			std::string m_game_id;
			cell_bound m_boundary; // boundary的长宽都需要大于四倍的ghost_radius
			std::array<space_node*, 2> m_children;
			space_node* m_parent = nullptr;
			bool m_ready = false;
			bool m_is_merging = false;
			bool m_is_split_x = false;
			std::array<float, 4> m_cell_loads;
			std::vector<entity_load> m_entity_loads;
			std::array<std::vector<std::uint16_t>,2> m_sorted_entity_load_idx_by_axis; // 存储m_entity_load数组的索引 使得这个数组对应的元素的pos按照坐标轴升序排列
			std::uint32_t m_cell_load_report_counter = 0; // 汇报负载的次数 每次boundary改变之后都要重置为0
		private:
			float m_total_cell_load; // 当前节点所有叶子节点的load总和
			float m_total_game_load; // 当前节点所有叶子节点的的game load总和
			std::vector<const space_node*> m_child_leaf;
			std::vector<std::string> m_child_games;
			std::uint32_t m_min_cell_load_report_counter;
		public:
			space_node(const cell_bound& in_bound, const std::string& in_game_id, const std::string& in_space_id, space_node* in_parent)
			: m_space_id(in_space_id)
			, m_game_id(in_game_id)
			, m_boundary(in_bound)
			, m_children{nullptr, nullptr}
			, m_parent(in_parent)
			{
				std::fill(m_cell_loads.begin(), m_cell_loads.end(), 0.0f);
			}
			space_node* split_x(double x, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id, const std::string& new_parent_space_id);
			space_node* split_z(double z, const std::string& new_space_game_id, const std::string& low_space_id, const std::string& up_space_id, const std::string& new_parent_space_id);
			// 将两个子节点的分界线调整为split_v
			bool balance(double split_v);
			bool is_leaf_cell() const
			{
				return !m_children[0];
			}
			bool ready() const
			{
				return m_ready;
			}
			bool is_merging() const
			{
				return m_is_merging;
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
			const std::array<space_node*, 2>& children() const
			{
				return m_children;
			}
			const space_node* sibling() const;

			space_node* parent()
			{
				return m_parent;
			}
			const space_node* parent() const
			{
				return m_parent;
			}
			// 当前节点的两个子节点合并 
			void merge_to_child(const std::string& dest);

			json encode() const;
			float get_smoothed_load() const;
			float get_latest_load() const;
			std::uint32_t cell_load_report_counter() const
			{
				return m_cell_load_report_counter;
			}
			void update_load(float cur_load, const std::vector<entity_load>& new_entity_loads);
			// 计算如果需要减少load_to_offset的负载，应该切分的位置
			// 保留长宽都要大于4*ghost_radius
			bool calc_offset_axis(float load_to_offset, double& out_split_axis, float& offseted_load, float ghost_radius) const;
			// 计算split时的最佳分割方向 每次都切分一个4*ghost_radius的区域 选择这个区域内负载最大的
			cell_split_direction calc_best_split_direction(float ghost_radius) const;

			// 计算当前节点的某个边界朝指定方向移动移动时的最大长度
			// 要求移动后任意子节点仍然有面积 这里暂时不考虑ghost_radius

			double calc_max_boundary_move_length(bool is_x, bool is_split_pos_smaller) const;

			// 递归的移动当前区域的某个边界 is_x代表坐标轴 is_split_pos_smaller代表是缩小还是放大
			// is_changing_max代表是否修改boundary.max还是min
			void update_boundary_with_new_split(double new_split_pos, bool is_x, bool is_split_pos_smaller, bool is_changing_max);

			// 计算在以这个新的分割轴进行分割的时候 能够缩小的entity_load总和
			float calc_move_split_offload(double new_split_pos, bool is_x, bool is_split_pos_smaller) const;

			bool check_can_shrink(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param, const double ghost_radius) const;

			const space_node* calc_shrink_node(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param, const double ghost_radius) const;

			double calc_best_shrink_new_split_pos(const cell_load_balance_param& lb_param, const double ghost_radius) const;
		private:
			bool set_child(int index, space_node* new_child);
			void set_ready();
			void set_is_merging();
			void on_split(int master_child_index);
			void make_sorted_loads();
			friend class space_cells;
			void update_load_stat(const std::unordered_map<std::string, float>& game_loads);
		};
	private:
		std::unordered_map<std::string, space_node*> m_leaf_nodes;
		std::unordered_map<std::string, space_node*> m_internal_nodes;
		// split与merge不会影响root_node
		space_node* m_root_node;
		std::uint64_t m_temp_node_counter = 0;
		// master 代表主逻辑cell 这个cell的生命周期伴随着整个space的生命周期
		// space的主体逻辑由master cell控制
		// 第一个创建的cell就是master_cell
		// 这个cell无法被合并到其他节点 同时也无法被负载均衡删除
		std::string m_master_cell_id;

		// 任何一个cell的长和宽必须大于等于四倍的ghost_radius
		double m_ghost_radius;

	public:
		// 选择一个合适的cell来分割 分割要求
		// 1. 这个cell所在的game load 要大于指定阈值
		// 2. 这个cell的load要大于指定阈值
		// 3. 长和宽至少有一个要大于8倍的ghost_radius 这样才能保证分割后的两个cell都有4倍radius
		// 选取cell load最大的
		const space_node* get_best_cell_to_split(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param) const;
		~space_cells();

		// 选择一个合适的cell来删除 删除要求
		// 1. 这个cell的负载要小于指定阈值 max_cell_load
		// 2. 选取其中 cell load最小的
		const space_node* get_best_cell_to_merge(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param);

		// 选择一个合适的node来缩容
		// 1. 这个node的平均game负载起码要大于指定阈值
		// 2. 这个node的平均game 负载起码要比其兄弟节点的平均负载大于指定阈值
		// 3. 这个cell的负载转移到兄弟节点之后 兄弟节点game的平均load 不能比当前game的平均load高
		// 优先选取底部节点
		const space_node* get_best_node_to_shrink(const std::unordered_map<std::string, float>& game_loads, const cell_load_balance_param& lb_param);

		

		// 计算一个节点 最大可能的shrink大小 这个节点可以是内部节点
		// shrink后需要保证里面所有的叶子节点的长宽都要有4*ghost_radius
		double calc_max_shrink_length(const space_node* shrink_node) const;


		
	public:
		
		space_cells(const cell_bound& bound, const std::string& game_id, const std::string& space_id, const double in_ghost_radius);
		const space_node* split_x(double x, const std::string& origin_space_id, const std::string& new_space_game_id, const std::string& left_space_id, const std::string& right_space_id);
		const space_node* split_z(double z, const std::string& origin_space_id, const std::string& new_space_game_id,const std::string& low_space_id, const std::string& high_space_id);
		const space_node* split_at_direction(const std::string& origin_space_id, cell_split_direction split_direction, const std::string& new_space_id, const std::string& new_space_game_id);
		// 将cell_id对应的cell 与其兄弟节点的分界线调整为split_v
		bool balance(double split_v, const std::string& cell_id);

		// 将某个内部节点的分割线移动到split_v
		bool balance(double split_v, const space_node* cur_node);

		// 将当前节点的boundary缩小到极限 并设置为is_merging
		bool start_merge(const std::string& cell_id);
		// 将space_id对应节点合并到space_id对应兄弟节点
		// 返回对应要删除node的game_id
		// 失败的情况下返回值都是空
		std::string finish_merge(const std::string& space_id);
		std::vector<const space_node*> query_intersect_leafs(const cell_bound& bound) const;
		const space_node* query_leaf_for_point(double x, double z) const;
		const std::unordered_map<std::string, space_node*>& cells() const
		{
			return m_leaf_nodes;
		}
		const space_node* get_leaf(const std::string& cell_id) const
		{
			auto cur_iter = m_leaf_nodes.find(cell_id);
			if(cur_iter == m_leaf_nodes.end())
			{
				return nullptr;
			}
			else
			{
				return cur_iter->second;
			}
		}

		const space_node* root_node() const
		{
			return m_root_node;
		}
		const auto& all_leafs() const
		{
			return m_leaf_nodes;
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

		void update_load_stat(const std::unordered_map<std::string, float>& game_loads);
	};
}