add_executable(content_validator main.cpp)

# soi_missions ships the mission and campaign loaders, and pulls in game_sim for
# the formation content pass: doctrines, unit layouts and troop role tags are
# validated by the same loader the game runs, so the validator can never drift
# from runtime behaviour.
#
# It links those loaders rather than compiling them again. Listing
# mission_loader.cpp and campaign_loader.cpp here as well used to leave two
# copies of every symbol in the link, resolved in favour of whichever the linker
# reached first -- so the validator could pass against a translation unit built
# with different flags from the one the game ships.
target_link_libraries(content_validator PRIVATE Qt${QT_VERSION_MAJOR}::Core soi_missions)

target_include_directories(content_validator PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(
    content_validator
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
