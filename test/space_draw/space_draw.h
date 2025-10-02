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



struct space_draw_config
{
	spiritsaway::shape_drawer::Color real_region_color; // real�������ɫ
	spiritsaway::shape_drawer::Color ghost_region_color; // ghost �������ɫ
	spiritsaway::shape_drawer::Color point_color; // entity�ı߿���������ɫ
	spiritsaway::shape_drawer::Color split_color; // cell�ָ��ߵ���ɫ
	std::uint16_t canvas_radius;  // ȫͼ�뾶
	
	std::string font_name; // ��������
	std::uint16_t font_size; // �����С
	std::uint16_t real_point_radius; // real entity�İ뾶
	std::uint16_t ghost_point_radius; // ÿһ��ghost������entity���ӵ�һ����Χ���Ķ���뾶
	std::uint16_t boundary_radius; // ȫͼ�ܱ����׵Ĳ���
	std::vector<spiritsaway::shape_drawer::Color> region_colors; // ����region����ʹ�õ���ɫ
	std::unordered_map<std::string, std::pair<std::string, std::string>> font_info; // �������������Ϣ
	std::uint16_t split_line_width; // �ָ��ߵĿ��
	bool with_entity_label; // �Ƿ����entity������

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(space_draw_config, real_region_color, ghost_region_color, point_color, split_color,  canvas_radius, font_name, font_size, real_point_radius, ghost_point_radius,  boundary_radius, region_colors, font_info);

};


json load_json_file(const std::string& file_path);

void draw_cell_region(const spiritsaway::utility::space_cells& cur_cell_region, const space_draw_config& draw_config, const std::string& folder_path, const std::string& file_name_prefix);