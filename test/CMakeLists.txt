set(TEST_SOURCES
    "chorematic_map_test.cpp"
    "maximum_weight_disk.cpp"
)

add_executable(chorematic_map_test chorematic_map_test.cpp ${TEST_SOURCES})
target_link_libraries(chorematic_map_test
    PRIVATE
    chorematic_map
)