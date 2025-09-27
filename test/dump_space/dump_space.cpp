#include "../space_draw/space_draw.h"

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