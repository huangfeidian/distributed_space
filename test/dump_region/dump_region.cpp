#include "drawer/png_drawer.h"
#include "basics/utf8_util.h"
#include "drawer/svg_drawer.h"
#include <nlohmann/json.hpp>
#include <iostream>
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

struct label_in_circle
{
	std::string u8_text;
	std::string font_name;
	Point center;
	std::uint32_t radius;
	Color color;
	std::uint16_t font_size;
	std::uint16_t stroke_width;
	void draw_png(PngImage& out_png) const
	{
		Circle cur_circle(radius, center, color, 1, false, stroke_width);
		out_png << cur_circle;
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_png.draw_text(cur_text_line, text, font_name, font_size, color, 1.0f);
	}

	void draw_svg(SvgGraph& out_svg)
	{
		Circle cur_circle(radius, center, color, 1, false, stroke_width);
		out_svg << cur_circle;
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_svg << LineText(cur_text_line, u8_text, font_name, font_size, color, 1.0f);
	}
};

struct LabelPoint
{
	std::string label;
	Point pos;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(LabelPoint, label, pos);
};

struct cell_space
{
	Point offset;
	Point min_xy;
	Point max_xy;

	std::uint16_t ghost_radius;
	std::vector<LabelPoint> points;

	Color real_color;
	Color ghost_color;

	Color real_point_color;
	Color ghost_point_color;

	std::uint16_t font_sz;
	std::string font_name;

	std::uint16_t point_radius;

	std::uint16_t point_circle_stroke;


	bool check_in_real_range(const Point& a) const
	{
		return a.x >= min_xy.x && a.x <= max_xy.x && a.y >= min_xy.y && a.y <= max_xy.y;
	}

	bool check_in_ghost_range(const Point& a) const
	{
		return a.x >= (min_xy.x - ghost_radius) && a.x <= (max_xy.x + ghost_radius) && a.y >= (min_xy.y-ghost_radius) && a.y <= (max_xy.y + ghost_radius);
	}

	void draw_png(PngImage& out_png) const
	{
		
		shape_drawer::Rectangle ghost_rect(min_xy + offset  - ghost_radius * Point(1, 1), max_xy -min_xy + ghost_radius * Point(2, 2), ghost_color, true, 0.4);
		ghost_rect.stroke_color = ghost_color;
		out_png << ghost_rect;

		shape_drawer::Rectangle real_rect(min_xy + offset, max_xy - min_xy, real_color,  true, 0.4);
		real_rect.stroke_color = real_color;
		out_png << real_rect;


		for (const auto& one_point : points)
		{
			if (check_in_ghost_range(one_point.pos))
			{
				label_in_circle cur_lable_circle;
				cur_lable_circle.center = one_point.pos + offset;
				cur_lable_circle.color = ghost_point_color;
				cur_lable_circle.font_name = font_name;
				cur_lable_circle.font_size = font_sz;
				cur_lable_circle.stroke_width = point_circle_stroke;
				cur_lable_circle.radius = point_radius;
				cur_lable_circle.u8_text = one_point.label;
				if (check_in_real_range(one_point.pos))
				{
					cur_lable_circle.color = real_point_color;
				}
				cur_lable_circle.draw_png(out_png);
			}
		}

	}

	void draw_svg(SvgGraph& out_svg)
	{
		shape_drawer::Rectangle ghost_rect(min_xy + offset - ghost_radius * Point(1, 1), max_xy - min_xy + ghost_radius * Point(2, 2), ghost_color, true, 0.4);
		ghost_rect.stroke_color = ghost_color;
		out_svg << ghost_rect;

		shape_drawer::Rectangle real_rect(min_xy + offset, max_xy - min_xy, real_color, true, 0.4);
		real_rect.stroke_color = real_color;
		out_svg << real_rect;


		for (const auto& one_point : points)
		{
			if (check_in_ghost_range(one_point.pos))
			{
				label_in_circle cur_lable_circle;
				cur_lable_circle.center = one_point.pos + offset;
				cur_lable_circle.color = ghost_point_color;
				cur_lable_circle.font_name = font_name;
				cur_lable_circle.font_size = font_sz;
				cur_lable_circle.stroke_width = point_circle_stroke;
				cur_lable_circle.radius = point_radius;
				cur_lable_circle.u8_text = one_point.label;
				if (check_in_real_range(one_point.pos))
				{
					cur_lable_circle.color = real_point_color;
				}
				cur_lable_circle.draw_svg(out_svg);
			}
		}
	}

};


struct cell_region_config
{
	Point min_xy;
	Point max_xy;
	Point offset;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(cell_region_config, min_xy, max_xy, offset);

	cell_region_config calc_union(const cell_region_config& other) const
	{
		auto result = *this;
		result.min_xy.x = std::min(result.min_xy.x, other.min_xy.x);
		result.min_xy.y = std::min(result.min_xy.y, other.min_xy.y);
		result.max_xy.x = std::max(result.max_xy.x, other.max_xy.x);
		result.max_xy.y = std::max(result.max_xy.y, other.max_xy.y);
		return result;
	}
};



struct cell_agents
{
	std::vector<cell_region_config> regions;
	std::vector< LabelPoint> points;

	Color real_region_color;
	Color ghost_region_color;
	Color real_point_color;
	Color ghost_point_color;
	std::string font_name;
	std::uint16_t font_size;
	std::uint16_t point_radius;
	std::uint16_t ghost_radius;
	std::uint16_t circle_stroke;
	std::uint16_t region_stroke;
	std::uint16_t boundary_radius;

	// Point offset;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(cell_agents, regions, points, real_region_color, ghost_region_color, real_point_color, ghost_point_color, font_name, font_size, point_radius, ghost_radius, circle_stroke, region_stroke, boundary_radius);


	Point space_origin_offset;
	Point space_min_xy_offset;

	Point calc_canvas()
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

		for (const auto& one_region : regions)
		{
			cell_region_config temp_region = one_region;
			temp_region.min_xy = one_region.offset;
			temp_region.max_xy = one_region.offset + one_region.max_xy - one_region.min_xy;
			full_region = full_region.calc_union(temp_region);
		}

		full_region.min_xy -= boundary_radius * Point(1, 1);
		full_region.max_xy += boundary_radius * Point(1, 1);
		space_min_xy_offset = -1 * full_region.min_xy;

		space_origin_offset = space_min_xy_offset - cur_aabb_region.min_xy;
		return full_region.max_xy - full_region.min_xy;
	}

	void draw_png(PngImage& out_png) const
	{
		for (auto one_region : regions)
		{
			
			one_region.min_xy += space_origin_offset;
			one_region.max_xy += space_origin_offset;
			shape_drawer::Rectangle cur_rect(one_region.min_xy, one_region.max_xy - one_region.min_xy, real_region_color, false);
			cur_rect.stroke_color = real_region_color;
			out_png << cur_rect;
		}
		for (auto one_point : points)
		{
			label_in_circle cur_shape;
			cur_shape.center = one_point.pos + space_origin_offset;
			cur_shape.u8_text = one_point.label;
			cur_shape.color = real_point_color;
			cur_shape.radius = point_radius;
			cur_shape.font_size = font_size;
			cur_shape.font_name = font_name;
			cur_shape.stroke_width = circle_stroke;
			cur_shape.draw_png(out_png);
		}

		for (const auto& one_region : regions)
		{
			cell_space cur_cell_space;
			cur_cell_space.font_name = font_name;
			cur_cell_space.font_sz = font_size;
			cur_cell_space.ghost_color = ghost_region_color;
			cur_cell_space.ghost_radius = ghost_radius;
			cur_cell_space.ghost_point_color = ghost_point_color;
			cur_cell_space.real_color = real_region_color;
			cur_cell_space.real_point_color = real_point_color;
			cur_cell_space.points = points;
			cur_cell_space.offset = space_min_xy_offset + one_region.offset - one_region.min_xy;
			cur_cell_space.max_xy = one_region.max_xy;
			cur_cell_space.min_xy = one_region.min_xy;
			cur_cell_space.point_radius = point_radius;
			cur_cell_space.point_circle_stroke = 1;
			cur_cell_space.draw_png(out_png);
		}
		
	}

	void draw_svg(SvgGraph& out_svg) const
	{
		for (auto one_region : regions)
		{

			one_region.min_xy += space_origin_offset;
			one_region.max_xy += space_origin_offset;
			shape_drawer::Rectangle cur_rect(one_region.min_xy, one_region.max_xy - one_region.min_xy, real_region_color, false, 1.0);
			cur_rect.stroke_color = real_region_color;

			out_svg << cur_rect;
		}
		for (auto one_point : points)
		{
			label_in_circle cur_shape;
			cur_shape.center = one_point.pos + space_origin_offset;
			cur_shape.u8_text = one_point.label;
			cur_shape.color = real_point_color;
			cur_shape.radius = point_radius;
			cur_shape.font_size = font_size;
			cur_shape.font_name = font_name;
			cur_shape.stroke_width = circle_stroke;
			cur_shape.draw_svg(out_svg);
		}

		for (const auto& one_region : regions)
		{
			cell_space cur_cell_space;
			cur_cell_space.font_name = font_name;
			cur_cell_space.font_sz = font_size;
			cur_cell_space.ghost_color = ghost_region_color;
			cur_cell_space.ghost_radius = ghost_radius;
			cur_cell_space.ghost_point_color = ghost_point_color;
			cur_cell_space.real_color = real_region_color;
			cur_cell_space.real_point_color = real_point_color;
			cur_cell_space.points = points;
			cur_cell_space.offset = space_min_xy_offset + one_region.offset - one_region.min_xy;
			cur_cell_space.max_xy = one_region.max_xy;
			cur_cell_space.min_xy = one_region.min_xy;
			cur_cell_space.point_radius = point_radius;
			cur_cell_space.point_circle_stroke = 1;
			cur_cell_space.draw_svg(out_svg);
		}
	}

};

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

int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		std::cout << "should provide file path" << std::endl;
		return 1;
	}
	auto cur_json = load_json_file(argv[1]);
	if (!cur_json.is_object())
	{
		std::cout << "json content should be object" << std::endl;
		return 1;
	}
	std::map<std::string, cell_agents> space_datas;
	std::unordered_map<string, std::pair<string, string>> font_info;
	cur_json.at("spaces").get_to(space_datas);
	cur_json.at("fonts").get_to(font_info);

	for (auto& one_pair : space_datas)
	{
		std::string dest_svg_file = one_pair.first + ".svg";
		std::string dest_png_file = one_pair.first + ".png";
		auto cur_canvas = one_pair.second.calc_canvas();
		std::int32_t cur_radius = std::max(cur_canvas.x, cur_canvas.y) / 2 + 10;
		PngImage png_image(font_info, dest_png_file, cur_radius, Color(255, 255, 255));
		SvgGraph svg_graph(font_info, dest_svg_file, cur_radius, Color(255, 255, 255));
		svg_graph.add_shape(one_pair.second);
		png_image.add_shape(one_pair.second);
	}
	return 1;
}