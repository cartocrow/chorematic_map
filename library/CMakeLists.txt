set(SOURCES
	<source>.cpp
)
set(HEADERS
	<header>.h
)

add_library(<module_name> ${SOURCES})
target_link_libraries(<module_name>
	PUBLIC 
	cartocrow::core	
)

#cartocrow_install_module(<module_name>)
install(FILES ${HEADERS} DESTINATION ${CARTOCROW_INSTALL_DIR}/<module_name>)