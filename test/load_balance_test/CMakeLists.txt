find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(load_balance_test load_balance_test.cpp)
target_link_libraries(load_balance_test PUBLIC space_draw spdlog::spdlog fmt::fmt)
