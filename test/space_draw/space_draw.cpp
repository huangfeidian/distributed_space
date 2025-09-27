#include "space_draw.h"

#include <fstream>
#include <streambuf>

using json = nlohmann::json;

using namespace spiritsaway::shape_drawer;
using namespace spiritsaway;

namespace spiritsaway::shape_drawer
{
	void to_json(json& j, const Point& p)
	{
		j = json{ std::array<int,2>{  p.x , p.y } };
	}

	void from_json(const json& k, Point& p)
	{
		std::array<int, 2> temp;
		k.get_to(temp);
		p.x = temp[0];
		p.y = temp[1];
	}

	void to_json(json& j, const Color& p)
	{
		j = json{ { "r", p.r }, { "g", p.g }, {"b", p.b} };
	}

	void from_json(const json& k, Color& p)
	{
		k.at("r").get_to(p.r);
		k.at("g").get_to(p.g);
		k.at("b").get_to(p.b);
	}
}


void label_in_circle::draw_png(PngImage& out_png) const
{
	Circle cur_circle(radius, center, color, 1, false, stroke_width);
	out_png << cur_circle;
	std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
	Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
	Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
	Line cur_text_line = Line(line_begin_p, line_end_p);
	out_png.draw_text(cur_text_line, text, font_name, font_size, color, 1.0f);
}

void label_in_circle::draw_svg(SvgGraph& out_svg) const
{
	Circle cur_circle(radius, center, color, 1, false, stroke_width);
	out_svg << cur_circle;
	std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
	Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
	Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
	Line cur_text_line = Line(line_begin_p, line_end_p);
	out_svg << LineText(cur_text_line, u8_text, font_name, font_size, color, 1.0f);
}






bool cell_space::check_in_real_range(const Point& a) const
{
	return a.x >= min_xy.x && a.x <= max_xy.x && a.y >= min_xy.y && a.y <= max_xy.y;
}

bool cell_space::check_in_ghost_range(const Point& a) const
{
	return a.x >= (min_xy.x - ghost_radius) && a.x <= (max_xy.x + ghost_radius) && a.y >= (min_xy.y - ghost_radius) && a.y <= (max_xy.y + ghost_radius);
}

void cell_space::draw_png(PngImage& out_png) const
{

	shape_drawer::Rectangle ghost_rect(min_xy + offset - ghost_radius * Point(1, 1), max_xy - min_xy + ghost_radius * Point(2, 2), ghost_color, true, 0.4);
	ghost_rect.stroke_color = ghost_color;
	out_png << ghost_rect;

	shape_drawer::Rectangle real_rect(min_xy + offset, max_xy - min_xy, real_color, true, 0.4);
	real_rect.stroke_color = real_color;
	out_png << real_rect;

}

void cell_space::draw_svg(SvgGraph& out_svg) const
{
	shape_drawer::Rectangle ghost_rect(min_xy + offset - ghost_radius * Point(1, 1), max_xy - min_xy + ghost_radius * Point(2, 2), ghost_color, true, 0.4);
	ghost_rect.stroke_color = ghost_color;
	out_svg << ghost_rect;

	shape_drawer::Rectangle real_rect(min_xy + offset, max_xy - min_xy, real_color, true, 0.4);
	real_rect.stroke_color = real_color;
	out_svg << real_rect;

}









void cell_agents::calc_region_adjacent_matrix()
{
	region_adjacent_matrix.resize(regions.size());
	for (int i = 0; i < regions.size(); i++)
	{
		region_adjacent_matrix[i].resize(regions.size(), 0);
	}
	for (int i = 0; i < regions.size(); i++)
	{
		for (int j = i + 1; j < regions.size(); j++)
		{
			if (regions[i].intersect(regions[j], draw_config.boundary_radius))
			{
				region_adjacent_matrix[i][j] = 1;
				region_adjacent_matrix[j][i] = 1;
			}
		}
	}

}
bool cell_agents::check_region_color_match(int target) const
{
	for (int i = 0; i < target; i++)
	{
		if (region_adjacent_matrix[i][target] && regions[i].color_idx == regions[target].color_idx)
		{
			return false;
		}
	}
	return true;
}
bool cell_agents::calc_region_colors_recursive(int i)
{

	for (int j = 0; j < draw_config.region_colors.size(); j++)
	{
		regions[i].color_idx = j;
		if (!check_region_color_match(i))
		{
			continue;
		}
		if (i == regions.size())
		{
			return true;
		}
		if (calc_region_colors_recursive(i + 1))
		{
			return true;
		}
	}
	return false;
}
bool cell_agents::calc_region_colors()
{
	for (int i = 0; i < regions.size(); i++)
	{
		regions[i].color_idx = 0;
	}
	return calc_region_colors_recursive(0);
}


void cell_agents::calc_agents()
{
	for (const auto& one_region : regions)
	{
		for (const auto& one_point : one_region.points)
		{
			auto& cur_item = agents[one_point.label];
			if (cur_item.region_colors.empty())
			{
				cur_item.name = one_point.label;
				cur_item.color = draw_config.point_color;
				cur_item.pos = one_point.pos;
				cur_item.font_name = draw_config.font_name;
				cur_item.font_size = draw_config.font_size;
			}
			cur_item.region_colors.push_back(draw_config.region_colors[one_region.color_idx]);
			if (one_point.is_real)
			{
				cur_item.radius += draw_config.real_point_radius;
			}
			else
			{
				cur_item.radius += draw_config.ghost_point_radius;
			}
			
		}
		
	}
}
Point cell_agents::calc_canvas()
{
	cell_region_config cur_aabb_region = regions[0];
	for (const auto& one_region : regions)
	{
		cur_aabb_region = cur_aabb_region.calc_union(one_region);
	}
	cell_region_config full_region;

	Point full_region_sz = cur_aabb_region.max_xy - cur_aabb_region.min_xy;
	full_region.max_xy = full_region_sz;
	full_region.min_xy.x = 0;
	full_region.min_xy.y = 0;

	full_region.min_xy -= draw_config.boundary_radius * Point(1, 1);
	full_region.max_xy += draw_config.boundary_radius * Point(1, 1);
	space_min_xy_offset = -1 * full_region.min_xy;

	space_origin_offset = space_min_xy_offset - cur_aabb_region.min_xy;
	return full_region.max_xy - full_region.min_xy;
}

void cell_agents::draw_png(PngImage& out_png) const
{
	for (auto one_region : regions)
	{

		one_region.min_xy += space_origin_offset;
		one_region.max_xy += space_origin_offset;
		shape_drawer::Rectangle cur_rect(one_region.min_xy, one_region.max_xy - one_region.min_xy, draw_config.region_colors[one_region.color_idx], false);
		cur_rect.stroke_color = draw_config.region_colors[one_region.color_idx];
		out_png << cur_rect;
	}
	for (const auto& one_region : regions)
	{
		cell_space cur_cell_space;
		cur_cell_space.font_name = draw_config.font_name;
		cur_cell_space.font_sz = draw_config.font_size;
		cur_cell_space.ghost_color = draw_config.ghost_region_color;
		cur_cell_space.ghost_radius = draw_config.ghost_radius;
		cur_cell_space.real_color = draw_config.real_region_color;

		cur_cell_space.offset = space_min_xy_offset;
		cur_cell_space.max_xy = one_region.max_xy;
		cur_cell_space.min_xy = one_region.min_xy;
		cur_cell_space.draw_png(out_png);
	}

}

void cell_agents::draw_svg(SvgGraph& out_svg) const
{
	for (auto one_region : regions)
	{

		one_region.min_xy += space_origin_offset;
		one_region.max_xy += space_origin_offset;
		shape_drawer::Rectangle cur_rect(one_region.min_xy, one_region.max_xy - one_region.min_xy, draw_config.region_colors[one_region.color_idx], false, 1.0);
		cur_rect.stroke_color = draw_config.region_colors[one_region.color_idx];

		out_svg << cur_rect;
	}

	for (const auto& one_region : regions)
	{
		cell_space cur_cell_space;
		cur_cell_space.font_name = draw_config.font_name;
		cur_cell_space.font_sz = draw_config.font_size;
		cur_cell_space.ghost_color = draw_config.ghost_region_color;
		cur_cell_space.ghost_radius = draw_config.ghost_radius;
		cur_cell_space.real_color = draw_config.real_region_color;
		cur_cell_space.offset = space_min_xy_offset;
		cur_cell_space.max_xy = one_region.max_xy;
		cur_cell_space.min_xy = one_region.min_xy;
		cur_cell_space.draw_svg(out_svg);
	}
}

void agent_info::draw_png(spiritsaway::shape_drawer::PngImage& out_png) const
{
	std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(name);
	auto text_center = pos + Point(0, radius);
	Point line_begin_p = text_center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
	Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
	Line cur_text_line = Line(line_begin_p, line_end_p);
	out_png << LineText(cur_text_line, name, font_name, font_size, color, 1.0f);
	spiritsaway::shape_drawer::Circle outline_circle(radius, pos, color, 1, true, 0);
	out_png << outline_circle;
	switch (region_colors.size())
	{
	case 1:
	{
		spiritsaway::shape_drawer::Rectangle rect1(pos - 0.7 * Point(radius, radius), 1.4 * Point(radius, radius), region_colors[0], true);
		out_png << rect1;
	}
		break;
	case 2:
	{
		auto rect_half_extent = 1.0 / std::sqrt(5);
		spiritsaway::shape_drawer::Rectangle rect1(pos - Point(2* rect_half_extent*radius, rect_half_extent* radius), Point(2*rect_half_extent*radius, 2*rect_half_extent*radius), region_colors[0], true);
		out_png << rect1;
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(-2 * rect_half_extent * radius, rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
		out_png << rect2;
	}
		break;
	case 3:
	{
		auto rect_half_extent = 0.5 / std::sqrt(2);
		spiritsaway::shape_drawer::Rectangle rect1(pos - Point(2 * rect_half_extent * radius, 2*rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[0], true);
		out_png << rect1;
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(0, 2* rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
		out_png << rect2;
		spiritsaway::shape_drawer::Rectangle rect3(pos - Point(-1 * rect_half_extent * radius, 0), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[2], true);
		out_png << rect3;

	}
		break;
	case 4:
	{
		auto rect_half_extent = 0.5 / std::sqrt(2);
		spiritsaway::shape_drawer::Rectangle rect1(pos - Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[0], true);
		out_png << rect1;
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(0, 2 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
		out_png << rect2;

		spiritsaway::shape_drawer::Rectangle rect3(pos - Point(2 * rect_half_extent * radius, 0), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[2], true);
		out_png << rect3;
		spiritsaway::shape_drawer::Rectangle rect4(pos - Point(0, 0), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[3], true);
		out_png << rect4;
	}
		break;
	default:
	{

	}

	}
}


void agent_info::draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const
{
	std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(name);
	auto text_center = pos + Point(0, radius);
	Point line_begin_p = text_center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
	Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
	Line cur_text_line = Line(line_begin_p, line_end_p);
	out_svg << LineText(cur_text_line, name, font_name, font_size, color, 1.0f);

	spiritsaway::shape_drawer::Circle outline_circle(radius, pos, color, 1, true, 0);
	out_svg << outline_circle;
	switch (region_colors.size())
	{
	case 1:
	{
		spiritsaway::shape_drawer::Rectangle rect1(pos - 0.7 * Point(radius, radius), 1.4 * Point(radius, radius), region_colors[0], true);
		out_svg << rect1;
	}
	break;
	case 2:
	{
		auto rect_half_extent = 1.0 / std::sqrt(5);
		spiritsaway::shape_drawer::Rectangle rect1(pos - Point(2 * rect_half_extent * radius, rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[0], true);
		out_svg << rect1;
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(-2 * rect_half_extent * radius, rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
		out_svg << rect2;
	}
	break;
	case 3:
	{
		auto rect_half_extent = 0.5 / std::sqrt(2);
		spiritsaway::shape_drawer::Rectangle rect1(pos - Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[0], true);
		out_svg << rect1;
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(0, 2 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
		out_svg << rect2;
		spiritsaway::shape_drawer::Rectangle rect3(pos - Point(-1 * rect_half_extent * radius, 0), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[2], true);
		out_svg << rect3;

	}
	break;
	case 4:
	{
		auto rect_half_extent = 0.5 / std::sqrt(2);
		spiritsaway::shape_drawer::Rectangle rect1(pos - Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[0], true);
		out_svg << rect1;
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(0, 2 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
		out_svg << rect2;

		spiritsaway::shape_drawer::Rectangle rect3(pos - Point(2 * rect_half_extent * radius, 0), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[2], true);
		out_svg << rect3;
		spiritsaway::shape_drawer::Rectangle rect4(pos - Point(0, 0), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[3], true);
		out_svg << rect4;
	}
	break;
	default:
	{

	}

	}
}


json load_json_file(const std::string& file_path)
{
	std::ifstream t(file_path);
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	if (!json::accept(str))
	{
		return {};
	}
	return json::parse(str);
}


void draw_cell_region(const spiritsaway::utility::cell_region& cur_cell_region, const space_draw_config& draw_config)
{
	cell_agents cur_cell_configuration;
	cur_cell_configuration.draw_config = draw_config;
	for (const auto& [one_cell_id, one_cell_ptr] : cur_cell_region.all_cells())
	{
		if (one_cell_ptr->is_leaf())
		{
			cell_region_config new_leaf_config;
			new_leaf_config.min_xy.x = one_cell_ptr->boundary().left_x;
			new_leaf_config.min_xy.y = one_cell_ptr->boundary().low_z;
			new_leaf_config.max_xy.x = one_cell_ptr->boundary().right_x;
			new_leaf_config.max_xy.y = one_cell_ptr->boundary().high_z;
			new_leaf_config.name = one_cell_ptr->game_id() + "::" + one_cell_ptr->space_id();
			new_leaf_config.points.reserve(one_cell_ptr->get_entity_loads().size());
			for (const auto& one_entity : one_cell_ptr->get_entity_loads())
			{
				LabelPoint temp_point;
				temp_point.is_real = one_entity.is_real;
				temp_point.label = one_entity.name;
				temp_point.pos.x = one_entity.pos[0];
				temp_point.pos.y = one_entity.pos[1];
				new_leaf_config.points.push_back(temp_point);
			}
			cur_cell_configuration.regions.push_back(new_leaf_config);
		}
		else
		{
			auto cur_child_aabb = one_cell_ptr->children()[0]->boundary();
			spiritsaway::shape_drawer::Line temp_split_line;
			if (one_cell_ptr->is_split_x())
			{
				temp_split_line.from.x = cur_child_aabb.right_x;
				temp_split_line.to.x = cur_child_aabb.right_x;
				temp_split_line.from.y = cur_child_aabb.low_z;
				temp_split_line.to.y = cur_child_aabb.high_z;

			}
			else
			{
				temp_split_line.from.y = cur_child_aabb.high_z;
				temp_split_line.to.y = cur_child_aabb.high_z;
				temp_split_line.from.x = cur_child_aabb.left_x;
				temp_split_line.to.x = cur_child_aabb.right_x;
			}
			cur_cell_configuration.split_lines.push_back(temp_split_line);
		}
	}
}