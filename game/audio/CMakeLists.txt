# The mastering chain -- limiter, loudness, dither -- with no dependency on Qt,
# the ECS or the audio device. It is a target of its own because the offline
# preview tool needs exactly this and nothing else: audio_master_preview used to
# compile audio_mastering.cpp into itself, so the curve a mastering change was
# auditioned against was not the curve the game played.
add_library(soi_audio_mastering STATIC audio_mastering.cpp)
target_include_directories(soi_audio_mastering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_library(
    audio_system
    STATIC
    audio_system.cpp
    audio_cues.cpp
    cue_trace.cpp
    loop_seam.cpp
    resampler.cpp
    sound.cpp
    music_player.cpp
    miniaudio_backend.cpp
    audio_event_handler.cpp
)

target_include_directories(audio_system PUBLIC .)
target_include_directories(audio_system SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(
    audio_system
    PUBLIC Qt${QT_VERSION_MAJOR}::Core engine_core soi_audio_mastering
)
