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
        -g
        -gsource-map
        -sASSERTIONS
)
