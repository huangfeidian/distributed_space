#include "space_draw.h"

using json = nlohmann::json;

using namespace spiritsaway::shape_drawer;
using namespace spiritsaway;

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

};

struct text_with_center
{
	spiritsaway::shape_drawer::Point center;
	std::uint16_t font_size;
	std::string font_name;
	std::string u8_text;
	spiritsaway::shape_drawer::Color color;

	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;
	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};

struct text_with_left
{
	spiritsaway::shape_drawer::Point left;
	std::uint16_t font_size;
	std::string font_name;
	std::string u8_text;
	spiritsaway::shape_drawer::Color color;
	std::uint32_t width() const;
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;
	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};

struct cell_space
{
	spiritsaway::shape_drawer::Point min_xy;
	spiritsaway::shape_drawer::Point max_xy;

	std::uint16_t ghost_radius;

	spiritsaway::shape_drawer::Color real_color;
	spiritsaway::shape_drawer::Color ghost_color;

	text_with_center label_info;


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
		spiritsaway::utility::cell_bound aabb1;
		aabb1.min.x = min_xy.x;
		aabb1.min.z = min_xy.y;
		aabb1.max.x = max_xy.x;
		aabb1.max.z = max_xy.y;

		spiritsaway::utility::cell_bound aabb2;
		aabb2.min.x = other.min_xy.x - boundary_radius;
		aabb2.min.z = other.min_xy.y - boundary_radius;
		aabb2.max.x = other.max_xy.x + boundary_radius;
		aabb2.max.z = other.max_xy.y + boundary_radius;

		return aabb1.intersect(aabb2);
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
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png, bool with_label) const;

	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg, bool with_label) const;
};

struct space_draw_container
{
	std::vector<cell_region_config> regions;
	std::vector<std::vector<std::uint8_t>> region_adjacent_matrix;

	std::unordered_map<std::string, agent_info> agents;
	std::vector<spiritsaway::shape_drawer::Line> split_lines;

	std::vector<std::string> cell_captions;
	space_draw_config draw_config;
	double draw_scale;
	double ghost_radius;
	spiritsaway::shape_drawer::Point region_min_xy;
	Point caption_begin_pos;
	void calc_region_adjacent_matrix();
	bool check_region_color_match(int target) const;
	bool calc_region_colors_recursive(int i);
	bool calc_region_colors();
	void calc_agents();
	void calc_canvas();
	void calc_split_lines(const spiritsaway::utility::space_cells& cur_cell_region);
	void calc_captions(const spiritsaway::utility::space_cells& cur_cell_region);
	Point convert_pos_to_canvas(const Point& origin) const;
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;
	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};