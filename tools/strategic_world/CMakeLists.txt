# Command-line queries over the strategic world: validation, march paths,
# supply lines, the campaign chronology and battlefield requests. It links
# soi_strategic only, so it runs without a GPU or the simulation.
add_executable(strategic_world main.cpp)
target_link_libraries(strategic_world PRIVATE Qt${QT_VERSION_MAJOR}::Core soi_strategic)
target_include_directories(strategic_world PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(strategic_world PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
