set(SOURCES
    <entry_point>.cpp
)

add_executable(<executable_name> ${SOURCES})

find_package(nlohmann_json REQUIRED)

target_link_libraries(
    <executable_name>
    PRIVATE
    cartocrow::core
    cartocrow::reader
    cartocrow::renderer
    CGAL::CGAL
    nlohmann_json::nlohmann_json
)

install(TARGETS <executable_name> DESTINATION ${INSTALL_BINARY_DIR})