set(SOURCES
    chorematic_map.cpp
)

add_executable(chorematic_map_wasm ${SOURCES})

target_link_libraries(
    chorematic_map_wasm
    PRIVATE
    cartocrow::core
    cartocrow::renderer
    cartocrow::reader
    chorematic_map
)

target_link_options(chorematic_map_wasm PRIVATE
    --emit-tsd "$<TARGET_FILE_DIR:chorematic_map_wasm>/chorematic_map_wasm.d.ts"
    -sMODULARIZE -sEXPORT_ES6
    -sALLOW_MEMORY_GROWTH
    # Only debug builds include debug information, a source map and assertions.
    # CartoCrowConfig.cmake adds -g to all targets, so release builds override it with -g0.
    "$<IF:$<CONFIG:Release,MinSizeRel>,-g0,-g;-gsource-map;-sASSERTIONS>"
)
