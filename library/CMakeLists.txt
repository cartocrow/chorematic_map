set(SHARED_SOURCES
	maximum_weight_disk.cpp
	sampler.h
	disk_area.cpp
	choropleth.cpp
	natural_breaks_external.cpp
	choropleth_disks.cpp
	geojson_parsing.cpp
)
set(SHARED_HEADERS
	maximum_weight_disk.h
	disk_area.h
	choropleth.h
	natural_breaks.h
	weighted_region_sample.h
	choropleth_disks.h
	geojson_parsing.h
)
set(NATIVE_SOURCES
	input_parsing.cpp
	parse_points.cpp
)
set(NATIVE_HEADERS
	input_parsing.h
	parse_points.h
)

if(EMSCRIPTEN)
set(SOURCES
	${SHARED_SOURCES}
)
set(HEADERS
	${SHARED_HEADERS}
)
else()
set(SOURCES
	${SHARED_SOURCES}
	${NATIVE_SOURCES}
)
set(HEADERS
	${SHARED_HEADERS}
	${NATIVE_HEADERS}
)
endif()

add_library(chorematic_map ${SOURCES})
if (EMSCRIPTEN)
target_link_libraries(chorematic_map
		PUBLIC
		cartocrow::core
		cartocrow::circle_segment_helpers
		cartocrow::reader
)
else()
target_link_libraries(chorematic_map
		PUBLIC
		cartocrow::core
		cartocrow::circle_segment_helpers
		cartocrow::reader
		GDAL::GDAL
)
endif()

install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/chorematic_map)
