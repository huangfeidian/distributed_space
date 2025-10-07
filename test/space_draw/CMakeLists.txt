

add_library(space_draw space_draw.cpp)
target_link_libraries(space_draw PUBLIC distributed_space shape_drawer any_container)
