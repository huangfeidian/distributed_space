#include "space_draw.h"

#include "space_draw_container.h"

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
		j = json{ std::array<int, 3>{p.r, p.g, p.b } };
	}

	void from_json(const json& k, Color& p)
	{
		std::array<int, 3> temp;
		k.get_to(temp);
		p.r = temp[0];
		p.g = temp[1];
		p.b = temp[2];
	}
}




void label_in_circle::draw_png(PngImage& out_png) const
{
	Circle cur_circle(radius, center, color, 1, false, stroke_width);
	out_png << cur_circle;
	if (!u8_text.empty())
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_png.draw_text(cur_text_line, text, font_name, font_size, color, 1.0f);
	}
	
}

void label_in_circle::draw_svg(SvgGraph& out_svg) const
{
	Circle cur_circle(radius, center, color, 1, false, stroke_width);
	out_svg << cur_circle;
	if (!u8_text.empty())
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_svg << LineText(cur_text_line, u8_text, font_name, font_size, color, 1.0f);
	}
	
}

void text_with_center::draw_png(spiritsaway::shape_drawer::PngImage& out_png) const
{
	if (!u8_text.empty())
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_png.draw_text(cur_text_line, text, font_name, font_size, color, 1.0f);
	}
}

void text_with_center::draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const
{
	if (!u8_text.empty())
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_svg << LineText(cur_text_line, u8_text, font_name, font_size, color, 1.0f);
	}
}

std::uint32_t text_with_left::width() const
{
	if (u8_text.empty())
	{
		return 0;
	}
	else
	{
		return string_util::utf8_util::utf8_to_uint(u8_text).size() * font_size;
	}
}
void text_with_left::draw_png(spiritsaway::shape_drawer::PngImage& out_png) const
{
	if (!u8_text.empty())
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = left;
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_png.draw_text(cur_text_line, text, font_name, font_size, color, 1.0f);
	}
}

void text_with_left::draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const
{
	if (!u8_text.empty())
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(u8_text);
		Point line_begin_p = left;
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_svg << LineText(cur_text_line, u8_text, font_name, font_size, color, 1.0f);
	}
}

void cell_space::draw_png(PngImage& out_png) const
{

	shape_drawer::Rectangle ghost_rect(min_xy  - ghost_radius * Point(1, 1), max_xy - min_xy + ghost_radius * Point(2, 2), ghost_color, true, 0.4);
	ghost_rect.stroke_color = ghost_color;
	out_png << ghost_rect;

	shape_drawer::Rectangle real_rect(min_xy, max_xy - min_xy, real_color, true, 0.4);
	real_rect.stroke_color = real_color;
	out_png << real_rect;

	label_info.draw_png(out_png);

}

void cell_space::draw_svg(SvgGraph& out_svg) const
{
	shape_drawer::Rectangle ghost_rect(min_xy - ghost_radius * Point(1, 1), max_xy - min_xy + ghost_radius * Point(2, 2), ghost_color, true, 0.4);
	ghost_rect.stroke_color = ghost_color;
	out_svg << ghost_rect;

	shape_drawer::Rectangle real_rect(min_xy , max_xy - min_xy, real_color, true, 0.4);
	real_rect.stroke_color = real_color;
	out_svg << real_rect;

	label_info.draw_svg(out_svg);
}


void space_draw_container::calc_region_adjacent_matrix()
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
			if (regions[i].intersect(regions[j], ghost_radius * draw_scale))
			{
				region_adjacent_matrix[i][j] = 1;
				region_adjacent_matrix[j][i] = 1;
			}
		}
	}

}
bool space_draw_container::check_region_color_match(int target) const
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
bool space_draw_container::calc_region_colors_recursive(int i)
{

	for (int j = 0; j < draw_config.region_colors.size(); j++)
	{
		regions[i].color_idx = (j + 1 + regions[i-1].color_idx)% draw_config.region_colors.size();
		if (!check_region_color_match(i))
		{
			continue;
		}
		if (i + 1 == regions.size())
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
bool space_draw_container::calc_region_colors()
{
	for (int i = 0; i < regions.size(); i++)
	{
		regions[i].color_idx = 0;
	}
	if (regions.size() == 1)
	{
		return true;
	}
	return calc_region_colors_recursive(1);
}


void space_draw_container::calc_agents()
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

void space_draw_container::calc_split_lines(const spiritsaway::utility::space_cells& cur_cell_region)
{
	auto cur_root_node = cur_cell_region.root_cell();
	std::vector<const spiritsaway::utility::space_cells::cell_node*> process_queue;
	process_queue.push_back(cur_root_node);
	while (!process_queue.empty())
	{
		auto cur_top = process_queue.back();
		process_queue.pop_back();
		std::string result_caption;
		if (!cur_top->is_leaf())
		{
			auto cur_child_aabb = cur_top->children()[0]->boundary();
			spiritsaway::shape_drawer::Line temp_split_line;
			if (cur_top->is_split_x())
			{
				temp_split_line.from.x = (int)cur_child_aabb.max.x;
				temp_split_line.to.x = (int)cur_child_aabb.max.x;
				temp_split_line.from.y = (int)cur_child_aabb.min.z;
				temp_split_line.to.y = (int)cur_child_aabb.max.z;

			}
			else
			{
				temp_split_line.from.y = (int)cur_child_aabb.max.z;
				temp_split_line.to.y = (int)cur_child_aabb.max.z;
				temp_split_line.from.x = (int)cur_child_aabb.min.x;
				temp_split_line.to.x = (int)cur_child_aabb.max.x;
			}
			temp_split_line.width = draw_config.split_line_width;
			temp_split_line.color = draw_config.split_color;
			split_lines.push_back(temp_split_line);
			process_queue.push_back(cur_top->children()[0]);
			process_queue.push_back(cur_top->children()[1]);
		}
	}
}
void space_draw_container::calc_canvas()
{
	cell_region_config cur_aabb_region = regions[0];
	for (const auto& one_region : regions)
	{
		cur_aabb_region = cur_aabb_region.calc_union(one_region);
	}
	region_min_xy = cur_aabb_region.min_xy;

	cell_region_config full_region;

	Point full_region_sz = cur_aabb_region.max_xy - cur_aabb_region.min_xy;
	draw_scale = 2 * (draw_config.canvas_radius * 1.0  - draw_config.boundary_radius)/ ((std::max(full_region_sz.x, full_region_sz.y)));
	
	for (auto& one_region : regions)
	{
		one_region.max_xy = convert_pos_to_canvas(one_region.max_xy);
		one_region.min_xy = convert_pos_to_canvas(one_region.min_xy);
		for (auto& one_point : one_region.points)
		{
			one_point.pos = convert_pos_to_canvas(one_point.pos);
		}
	}
	for (auto& one_line : split_lines)
	{
		one_line.from = convert_pos_to_canvas(one_line.from);
		one_line.to = convert_pos_to_canvas(one_line.to);
	}
	caption_begin_pos = convert_pos_to_canvas(Point(cur_aabb_region.min_xy.x, cur_aabb_region.max_xy.y));
	caption_begin_pos.y += 2* draw_config.font_size + draw_config.boundary_radius;
}

Point space_draw_container::convert_pos_to_canvas(const Point& origin_pos) const
{
	Point result(draw_config.boundary_radius, draw_config.boundary_radius);
	result += (origin_pos - region_min_xy) * draw_scale;
	return result;
}

void space_draw_container::draw_png(PngImage& out_png) const
{
	//for (auto one_region : regions)
	//{
	//	shape_drawer::Rectangle cur_rect(one_region.min_xy, one_region.max_xy - one_region.min_xy, draw_config.region_colors[one_region.color_idx], false);
	//	cur_rect.stroke_color = draw_config.region_colors[one_region.color_idx];
	//	out_png << cur_rect;
	//}
	for (const auto& one_region : regions)
	{
		cell_space cur_cell_space;
		cur_cell_space.label_info.font_name = draw_config.font_name;
		cur_cell_space.label_info.font_size = draw_config.font_size;
		cur_cell_space.label_info.u8_text = one_region.name;
		cur_cell_space.label_info.color = draw_config.region_label_color;
		cur_cell_space.label_info.center = (one_region.max_xy + one_region.min_xy) / 2;
		cur_cell_space.ghost_color = draw_config.ghost_region_color;
		cur_cell_space.ghost_radius = ghost_radius * draw_scale;
		cur_cell_space.real_color = draw_config.region_colors[one_region.color_idx];

		cur_cell_space.max_xy = one_region.max_xy;
		cur_cell_space.min_xy = one_region.min_xy;
		cur_cell_space.draw_png(out_png);
	}
	for (const auto& one_split_line : split_lines)
	{
		out_png << one_split_line;
	}
	for (const auto& [one_agent_label, one_agent_info] : agents)
	{
		one_agent_info.draw_png(out_png, draw_config.with_entity_label);
	}
	auto cur_caption_pos = caption_begin_pos;
	for (const auto& one_caption : cell_captions)
	{
		if (cur_caption_pos.y >= 2 * draw_config.canvas_radius)
		{
			break;
		}
		text_with_left cur_text_line;
		cur_text_line.left = cur_caption_pos;
		cur_text_line.color = draw_config.region_label_color;
		cur_text_line.font_name = draw_config.font_name;
		cur_text_line.u8_text = one_caption;
		cur_text_line.font_size = draw_config.font_size;
		cur_text_line.draw_png(out_png);
		cur_caption_pos.y += 2 * draw_config.font_size;
	}
}

void space_draw_container::draw_svg(SvgGraph& out_svg) const
{
	//for (auto one_region : regions)
	//{
	//	shape_drawer::Rectangle cur_rect(one_region.min_xy, one_region.max_xy - one_region.min_xy, draw_config.region_colors[one_region.color_idx], false, 1.0);
	//	cur_rect.stroke_color = draw_config.region_colors[one_region.color_idx];

	//	out_svg << cur_rect;
	//}

	for (const auto& one_region : regions)
	{
		cell_space cur_cell_space;
		cur_cell_space.label_info.font_name = draw_config.font_name;
		cur_cell_space.label_info.font_size = draw_config.font_size;
		cur_cell_space.label_info.u8_text = one_region.name;
		cur_cell_space.label_info.color = draw_config.region_label_color;
		cur_cell_space.label_info.center = (one_region.max_xy + one_region.min_xy) / 2;
		cur_cell_space.ghost_color = draw_config.ghost_region_color;
		cur_cell_space.ghost_radius = ghost_radius * draw_scale;
		cur_cell_space.real_color = draw_config.region_colors[one_region.color_idx];
		cur_cell_space.max_xy = one_region.max_xy;
		cur_cell_space.min_xy = one_region.min_xy;
		cur_cell_space.draw_svg(out_svg);
	}

	for (const auto& one_split_line : split_lines)
	{
		out_svg << one_split_line;
	}

	for (const auto& [one_agent_label, one_agent_info] : agents)
	{
		one_agent_info.draw_svg(out_svg, draw_config.with_entity_label);
	}

	auto cur_caption_pos = caption_begin_pos;
	for (const auto& one_caption : cell_captions)
	{
		if (cur_caption_pos.y >= 2 * draw_config.canvas_radius)
		{
			break;
		}
		text_with_left cur_text_line;
		cur_text_line.left = cur_caption_pos;
		cur_text_line.color = draw_config.point_color;
		cur_text_line.font_name = draw_config.font_name;
		cur_text_line.u8_text = one_caption;
		cur_text_line.font_size = draw_config.font_size;
		cur_text_line.draw_svg(out_svg);
		cur_caption_pos.y += 2 * draw_config.font_size;
	}
}

void agent_info::draw_png(spiritsaway::shape_drawer::PngImage& out_png, bool with_label) const
{
	if (with_label)
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(name);
		auto text_center = pos + Point(0, radius);
		Point line_begin_p = text_center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_png << LineText(cur_text_line, name, font_name, font_size, color, 1.0f);
	}
	
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
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(0, rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
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
		spiritsaway::shape_drawer::Rectangle rect3(pos - Point(-1 * rect_half_extent * radius, -1 * rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[2], true);
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


void agent_info::draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg, bool with_label) const
{
	if (with_label)
	{
		std::vector<std::uint32_t> text = string_util::utf8_util::utf8_to_uint(name);
		auto text_center = pos + Point(0, radius);
		Point line_begin_p = text_center - text.size() * 0.25 * font_size * Point(1, 0) + 0.25 * font_size * Point(0, 1);
		Point line_end_p = line_begin_p + text.size() * font_size * Point(1, 0);
		Line cur_text_line = Line(line_begin_p, line_end_p);
		out_svg << LineText(cur_text_line, name, font_name, font_size, color, 1.0f);
	}
	

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
		spiritsaway::shape_drawer::Rectangle rect2(pos - Point(0, rect_half_extent * radius), Point(2 * rect_half_extent * radius, 2 * rect_half_extent * radius), region_colors[1], true);
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

void space_draw_container::calc_captions(const spiritsaway::utility::space_cells& cur_cell_region)
{
	auto cur_root_node = cur_cell_region.root_cell();
	std::vector<const spiritsaway::utility::space_cells::cell_node*> process_queue;
	process_queue.push_back(cur_root_node);
	while (!process_queue.empty())
	{
		auto cur_top = process_queue.back();
		process_queue.pop_back();
		std::string result_caption;
		if (cur_top->is_leaf())
		{
			result_caption += "LeafNode-" + cur_top->space_id() +  " Boundary(" + json(cur_top->boundary()).dump() + ") " + " Game(" + cur_top->game_id() + ") ";
			int real_num = 0;
			int ghost_num = 0;
			for (const auto& one_entity : cur_top->get_entity_loads())
			{
				if (one_entity.is_real)
				{
					real_num++;
				}
				else
				{
					ghost_num++;
				}
			}
			result_caption += "real(" + std::to_string(real_num) + ") ghost(" + std::to_string(ghost_num) + ") load(" + std::to_string(int(cur_top->get_smoothed_load())) + ") ";
			if (cur_top->parent())
			{
				result_caption += " Parent(InternalNode-" + cur_top->parent()->space_id() + ")";
			}
			cell_captions.push_back(result_caption);
		}
		else
		{
			result_caption += "InternalNode-" + cur_top->space_id()  +" Boundary(" + json(cur_top->boundary()).dump() + ") ";
			result_caption += " IsSplitX(" + std::to_string(cur_top->is_split_x()) + ") ";
			for (int i = 0; i < 2; i++)
			{
				result_caption += " Child" + std::to_string(i) + "(";
				auto cur_child = cur_top->children()[i];
				if (cur_child->is_leaf())
				{
					result_caption += "LeafNode-" + cur_child->space_id();
				}
				else
				{
					result_caption += "InternalNode-" + cur_child->space_id();
				}
				result_caption += ") ";
			}
			
			cell_captions.push_back(result_caption);
			process_queue.push_back(cur_top->children()[0]);
			process_queue.push_back(cur_top->children()[1]);

		}
	}
}
void draw_cell_region(const spiritsaway::utility::space_cells& cur_cell_region, const space_draw_config& draw_config, const std::string& folder_path, const std::string& file_name_prefix)
{
	space_draw_container cur_cell_configuration;
	cur_cell_configuration.draw_config = draw_config;
	cur_cell_configuration.ghost_radius = cur_cell_region.ghost_radius();
	for (const auto& [one_cell_id, one_cell_ptr] : cur_cell_region.all_cells())
	{
		if (one_cell_ptr->is_leaf())
		{
			cell_region_config new_leaf_config;
			new_leaf_config.min_xy.x = one_cell_ptr->boundary().min.x;
			new_leaf_config.min_xy.y = one_cell_ptr->boundary().min.z;
			new_leaf_config.max_xy.x = one_cell_ptr->boundary().max.x;
			new_leaf_config.max_xy.y = one_cell_ptr->boundary().max.z;
			int real_num = 0;
			int ghost_num = 0;
			new_leaf_config.points.reserve(one_cell_ptr->get_entity_loads().size());
			for (const auto& one_entity : one_cell_ptr->get_entity_loads())
			{
				LabelPoint temp_point;
				temp_point.is_real = one_entity.is_real;
				temp_point.label = one_entity.name;
				temp_point.pos.x = one_entity.pos[0];
				temp_point.pos.y = one_entity.pos[1];
				new_leaf_config.points.push_back(temp_point);
				if (one_entity.is_real)
				{
					real_num++;
				}
				else
				{
					ghost_num++;
				}
			}
			new_leaf_config.name = one_cell_ptr->space_id();
			cur_cell_configuration.regions.push_back(new_leaf_config);
		}
	}
	cur_cell_configuration.calc_split_lines(cur_cell_region);
	cur_cell_configuration.calc_canvas();
	cur_cell_configuration.calc_region_adjacent_matrix();
	cur_cell_configuration.calc_region_colors();
	cur_cell_configuration.calc_agents();
	cur_cell_configuration.calc_captions(cur_cell_region);
	auto cur_png = PngImage(draw_config.font_info, folder_path + "/" + file_name_prefix + ".png", draw_config.canvas_radius, Color(255, 255, 255));
	auto cur_svg = SvgGraph(draw_config.font_info, folder_path + "/" + file_name_prefix + ".svg", draw_config.canvas_radius, Color(255, 255, 255));

	cur_cell_configuration.draw_png(cur_png);
	cur_cell_configuration.draw_svg(cur_svg);
}