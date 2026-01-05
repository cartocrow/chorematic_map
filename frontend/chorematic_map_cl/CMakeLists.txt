set(SOURCES
    chorematic_map_cl.cpp
)

add_executable(chorematic_map_cl ${SOURCES})

find_package(nlohmann_json REQUIRED)

target_link_libraries(
    chorematic_map_cl
    PRIVATE
    chorematic_map
    cartocrow::core
    cartocrow::reader
    cartocrow::renderer
    CGAL::CGAL
    nlohmann_json::nlohmann_json
)

install(TARGETS chorematic_map_cl DESTINATION ${INSTALL_BINARY_DIR})
