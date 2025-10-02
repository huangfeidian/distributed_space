#include "../space_draw/space_draw.h"
#include <chrono>

using namespace spiritsaway::utility;

std::string format_timepoint(std::uint64_t milliseconds_since_epoch)
{
	auto epoch_begin = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>();
	auto cur_timepoint = epoch_begin + std::chrono::milliseconds(milliseconds_since_epoch);
	auto cur_time_t = std::chrono::system_clock::to_time_t(cur_timepoint);

	struct tm* timeinfo;
	char buffer[80];

	timeinfo = localtime(&cur_time_t);

	strftime(buffer, sizeof(buffer), "%Y_%m_%d_%H_%M_%S", timeinfo);
	return std::string(buffer);
}

int main(int argc, const char** argv)
{
	if (argc < 3)
	{
		std::cout << "should provide space_data json and draw_config json" << std::endl;
		return 1;
	}
	auto space_data_json = load_json_file(argv[1]);
	if (space_data_json.is_null())
	{
		std::cout << "fail to load json from " << argv[1] << std::endl;
		return 1;
	}
	space_cells cur_space_cells(space_cells::cell_bound{}, std::string{}, std::string{});
	if (!cur_space_cells.decode(space_data_json))
	{
		std::cout << "fail to decode space_cells from " << space_data_json << std::endl;
		return 1;
	}
	auto draw_config_json = load_json_file(argv[2]);
	if (draw_config_json.is_null())
	{
		std::cout << "fail to load json from " << argv[2] << std::endl;
		return 1;
	}
	space_draw_config cur_draw_config;
	try
	{
		draw_config_json.get_to(cur_draw_config);
	}
	catch (std::exception& e)
	{
		std::cout << "fail to decode space_draw_config from " << draw_config_json << " exception "<<e.what()<<std::endl;
		return 1;
	}
	auto microsecondsUTC = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	auto cur_file_name = "dump_space_" + format_timepoint(microsecondsUTC);
	auto folder_name = "test_draw";
	draw_cell_region(cur_space_cells, cur_draw_config, folder_name, cur_file_name);
	return 1;
}