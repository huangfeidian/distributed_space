#pragma once

#include "drawer/png_drawer.h"
#include "basics/utf8_util.h"
#include "drawer/svg_drawer.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include "cell_region.h"

using json = nlohmann::json;

namespace spiritsaway::shape_drawer
{
	void to_json(json& j, const Point& p);


	void from_json(const json& k, Point& p);


	void to_json(json& j, const Color& p);


	void from_json(const json& k, Color& p);

}

struct label_in_circle
{
	std::string u8_text;
	std::string font_name;
	spiritsaway::shape_drawer::Point center;
	std::uint32_t radius;
	spiritsaway::shape_drawer::Color color;
	std::uint16_t font_size;
	std::uint16_t stroke_width;
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;
	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;

};

struct LabelPoint
{
	std::string label;
	spiritsaway::shape_drawer::Point pos;
	bool is_real;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(LabelPoint, label, pos);
};


struct cell_space
{
	spiritsaway::shape_drawer::Point offset;
	spiritsaway::shape_drawer::Point min_xy;
	spiritsaway::shape_drawer::Point max_xy;

	std::uint16_t ghost_radius;

	spiritsaway::shape_drawer::Color real_color;
	spiritsaway::shape_drawer::Color ghost_color;

	std::uint16_t font_sz;
	std::string font_name;


	bool check_in_real_range(const spiritsaway::shape_drawer::Point& a) const;


	bool check_in_ghost_range(const spiritsaway::shape_drawer::Point& a) const;


	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;

	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};

struct cell_region_config
{
	spiritsaway::shape_drawer::Point min_xy;
	spiritsaway::shape_drawer::Point max_xy;
	std::vector< LabelPoint> points;
	std::string name;
	int color_idx;

	cell_region_config calc_union(const cell_region_config& other) const
	{
		auto result = *this;
		result.min_xy.x = std::min(result.min_xy.x, other.min_xy.x);
		result.min_xy.y = std::min(result.min_xy.y, other.min_xy.y);
		result.max_xy.x = std::max(result.max_xy.x, other.max_xy.x);
		result.max_xy.y = std::max(result.max_xy.y, other.max_xy.y);
		return result;
	}
	bool intersect(const cell_region_config& other, float boundary_radius) const
	{
		auto new_region = calc_union(other);
		auto region_diff = new_region.max_xy - new_region.min_xy;
		return region_diff.x <= boundary_radius && region_diff.y <= boundary_radius;
	}
};

struct agent_info
{
	std::vector<spiritsaway::shape_drawer::Color> region_colors;
	spiritsaway::shape_drawer::Point pos;
	spiritsaway::shape_drawer::Color color;
	int radius;
	std::string name;
	std::string font_name;
	std::uint16_t font_size;
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;

	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};

struct space_draw_config
{
	spiritsaway::shape_drawer::Color real_region_color; // real区域的颜色
	spiritsaway::shape_drawer::Color ghost_region_color; // ghost 区域的颜色
	spiritsaway::shape_drawer::Color point_color; // entity的边框与文字颜色
	spiritsaway::shape_drawer::Color split_color; // cell分割线的颜色
	std::uint16_t canvas_width;  // 全图宽度
	std::uint16_t canvas_height; // 全图高度
	std::string font_name; // 字体名字
	std::uint16_t font_size; // 字体大小
	std::uint16_t real_point_radius; // real entity的半径
	std::uint16_t ghost_point_radius; // 每一层ghost会给这个entity增加的一个外围环的额外半径
	std::uint16_t ghost_radius; // region的ghost半径
	std::uint16_t boundary_radius; // 全图周边留白的部分
	std::vector<spiritsaway::shape_drawer::Color> region_colors; // 所有region可以使用的颜色
};
struct cell_agents
{
	std::vector<cell_region_config> regions;
	std::vector<std::vector<std::uint8_t>> region_adjacent_matrix;
	
	std::unordered_map<std::string, agent_info> agents;
	std::vector<spiritsaway::shape_drawer::Line> split_lines;

	space_draw_config draw_config;


	spiritsaway::shape_drawer::Point space_origin_offset;
	spiritsaway::shape_drawer::Point space_min_xy_offset;

	void calc_region_adjacent_matrix();
	bool check_region_color_match(int target) const;
	bool calc_region_colors_recursive(int i);
	bool calc_region_colors();
	void calc_agents();
	spiritsaway::shape_drawer::Point calc_canvas();
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;
	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};


json load_json_file(const std::string& file_path);

void draw_cell_region(const spiritsaway::utility::cell_region& cur_cell_region, const space_draw_config& draw_config);