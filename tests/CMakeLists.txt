# Test suite for Standard of Iron
# Uses Google Test framework

# Test executable
add_executable(standard_of_iron_tests
    core/serialization_test.cpp
    db/save_storage_test.cpp
    render/pose_controller_test.cpp
    render/pose_controller_compatibility_test.cpp
    render/body_frames_test.cpp
    render/equipment_registry_test.cpp
    render/helmet_renderers_test.cpp
    render/armor_renderer_test.cpp
)

# Link against GTest, project libraries
target_link_libraries(standard_of_iron_tests
    PRIVATE
        GTest::gtest_main
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Sql
        engine_core
        game_systems
        render_gl
)

# Include directories
target_include_directories(standard_of_iron_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/game
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(standard_of_iron_tests)

# Set output directory
set_target_properties(standard_of_iron_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
