add_executable(movement_trace_report main.cpp)
target_link_libraries(movement_trace_report PRIVATE engine_core Qt${QT_VERSION_MAJOR}::Core)
target_include_directories(movement_trace_report PRIVATE ${CMAKE_SOURCE_DIR}/game)
set_target_properties(
    movement_trace_report
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
