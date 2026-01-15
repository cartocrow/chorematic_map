set(SOURCES
    chorematic_map_gui.cpp
)

add_executable(chorematic_map_gui ${SOURCES})

target_link_libraries(
    chorematic_map_gui
    PRIVATE
    chorematic_map
    cartocrow::core
    cartocrow::renderer
    cartocrow::reader
    cartocrow::widgets
    CGAL::CGAL
    GDAL::GDAL
    Qt5::Widgets
)

install(TARGETS chorematic_map_gui DESTINATION ${INSTALL_BINARY_DIR})