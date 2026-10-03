# ---- persistence_tests -------------------------------------------------
# What a save is, and whether the thing that writes it agrees. No runtime and no
# AI, because a mission snapshot must be complete without either.
add_executable(
    persistence_tests
    save/snapshot_contract_test.cpp
    save/serialization_golden_test.cpp
    core/serialization_test.cpp
    core/save_capture_coherence_test.cpp
    core/save_load_render_snapshot_test.cpp
    core/commander_state_round_trip_test.cpp
    db/save_storage_test.cpp
    db/save_format_test.cpp
    db/save_load_service_test.cpp
    db/mission_progress_test.cpp
    db/campaign_progression_test.cpp
    db/campaign_end_to_end_test.cpp
    headless/headless_main.cpp
)
target_link_libraries(
    persistence_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        soi_persistence
        soi_missions
)
soi_register_test_binary(persistence_tests)
