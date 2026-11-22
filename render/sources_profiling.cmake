# render/profiling: frame and asset counters. See scripts/check-render-layers.py.
set(RENDER_PROFILING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/allocation_tracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/asset_counters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/frame_continuity_analyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/frame_profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/gl_creation_trace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/performance_report.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling/profiling_hud.cpp
)
