# ---- campaign_tests ----------------------------------------------------
# Mission and campaign content -- what the shipped files say, and whether the
# loaders agree with them -- and the runtime that applies that content to a live
# world.
#
# It links no client on purpose: no renderer, no camera, no view models. Setting
# a mission up, spawning the forces it names, stepping the tutorial and firing
# its waves are world manipulation, and this link line is what says so. Those
# tests used to sit in app_tests only because the code did.
#
# soi_ai and soi_persistence arrive through soi_mission_runtime, which needs
# them: a definition names its opponents with a strategy, and a campaign records
# which missions are complete. Reading content still needs neither -- that half
# is soi_missions, and it links neither.
add_executable(
    campaign_tests
    map/mission_loader_test.cpp
    map/mission_asset_rules_test.cpp
    map/mission_stage_tracker_test.cpp
    map/commander_message_director_test.cpp
    map/commander_voice_bank_test.cpp
    map/mission_victory_rules_test.cpp
    map/wave_archetype_catalog_test.cpp
    map/difficulty_profile_test.cpp
    map/map_catalog_test.cpp
    map/mission_catalog_test.cpp
    map/mission_map_reachability_test.cpp
    map/campaign_loader_test.cpp
    map/campaign_roster_test.cpp
    map/campaign_content_integration_test.cpp
    map/campaign_province_data_test.cpp
    # The mission runtime, checked without a client. These were in app_tests
    # while the code they exercise sat in app/mission/; running them here is what
    # proves the move -- setting a mission up, spawning its forces, stepping the
    # tutorial and firing its waves need no renderer and no view model.
    core/campaign_manager_test.cpp
    core/commander_speaker_roster_test.cpp
    core/commander_voice_observer_test.cpp
    core/historical_commander_test.cpp
    core/mission_commander_setup_test.cpp
    core/mission_definition_view_test.cpp
    core/mission_events_test.cpp
    core/mission_wave_director_test.cpp
    core/tutorial_director_test.cpp
    simulation_main.cpp
)
target_link_libraries(
    campaign_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        soi_missions
        soi_mission_runtime
)
soi_register_test_binary(campaign_tests)
