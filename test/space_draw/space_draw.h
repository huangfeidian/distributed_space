#pragma once

#include "drawer/png_drawer.h"
#include "basics/utf8_util.h"
#include "drawer/svg_drawer.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include "space_cells.h"

using json = nlohmann::json;

namespace spiritsaway::shape_drawer
{
	void to_json(json& j, const Point& p);


	void from_json(const json& k, Point& p);


	void to_json(json& j, const Color& p);


	void from_json(const json& k, Color& p);

}



struct space_draw_config
{
	spiritsaway::shape_drawer::Color real_region_color; // real区域的颜色
	spiritsaway::shape_drawer::Color ghost_region_color; // ghost 区域的颜色
	spiritsaway::shape_drawer::Color point_color; // entity的边框与文字颜色
	spiritsaway::shape_drawer::Color split_color; // cell分割线的颜色
	spiritsaway::shape_drawer::Color region_label_color; // 区域名字的颜色
	std::uint16_t canvas_radius;  // 全图半径
	
	std::string font_name; // 字体名字
	std::uint16_t font_size; // 字体大小
	std::uint16_t real_point_radius; // real entity的半径
	std::uint16_t ghost_point_radius; // 每一层ghost会给这个entity增加的一个外围环的额外半径
	std::uint16_t boundary_radius; // 全图周边留白的部分
	std::vector<spiritsaway::shape_drawer::Color> region_colors; // 所有region可以使用的颜色
	std::unordered_map<std::string, std::pair<std::string, std::string>> font_info; // 所有相关字体信息
	std::uint16_t split_line_width; // 分割线的宽度
	bool with_entity_label; // 是否绘制entity的名字

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(space_draw_config, real_region_color, ghost_region_color, point_color, split_color,  canvas_radius, font_name, font_size, real_point_radius, ghost_point_radius,  boundary_radius, region_colors, font_info, region_label_color, with_entity_label, split_line_width);

};


json load_json_file(const std::string& file_path);

void draw_cell_region(const spiritsaway::distributed_space::space_cells& cur_cell_region, const space_draw_config& draw_config, const std::string& folder_path, const std::string& file_name_prefix);