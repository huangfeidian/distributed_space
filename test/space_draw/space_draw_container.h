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

struct cell_space
{
	spiritsaway::shape_drawer::Point min_xy;
	spiritsaway::shape_drawer::Point max_xy;

	std::uint16_t ghost_radius;

	spiritsaway::shape_drawer::Color real_color;
	spiritsaway::shape_drawer::Color ghost_color;

	std::uint16_t font_sz;
	std::string font_name;



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
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png, bool with_label) const;

	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg, bool with_label) const;
};

struct space_draw_container
{
	std::vector<cell_region_config> regions;
	std::vector<std::vector<std::uint8_t>> region_adjacent_matrix;

	std::unordered_map<std::string, agent_info> agents;
	std::vector<spiritsaway::shape_drawer::Line> split_lines;

	space_draw_config draw_config;
	double draw_scale;

	// spiritsaway::shape_drawer::Point space_origin_offset;
	spiritsaway::shape_drawer::Point region_min_xy;

	void calc_region_adjacent_matrix();
	bool check_region_color_match(int target) const;
	bool calc_region_colors_recursive(int i);
	bool calc_region_colors();
	void calc_agents();
	void calc_canvas();
	Point convert_pos_to_canvas(const Point& origin) const;
	void draw_png(spiritsaway::shape_drawer::PngImage& out_png) const;
	void draw_svg(spiritsaway::shape_drawer::SvgGraph& out_svg) const;
};