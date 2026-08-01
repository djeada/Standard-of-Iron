add_executable(
    content_validator
    main.cpp
    ../../game/map/mission_loader.cpp
    ../../game/map/campaign_loader.cpp
)

# game_sim is linked for the formation content pass: doctrines, unit layouts and
# troop role tags are validated by the same loader the game runs, so the
# validator can never drift from runtime behaviour.
target_link_libraries(content_validator PRIVATE Qt${QT_VERSION_MAJOR}::Core game_sim)

target_include_directories(content_validator PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(
    content_validator
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
