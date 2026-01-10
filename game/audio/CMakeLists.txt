add_library(audio_system STATIC
    audio_system.cpp
    sound.cpp
    music_player.cpp
    miniaudio_backend.cpp
    audio_event_handler.cpp
)

target_include_directories(audio_system PUBLIC .)
target_include_directories(audio_system SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(audio_system PUBLIC Qt${QT_VERSION_MAJOR}::Core engine_core)
