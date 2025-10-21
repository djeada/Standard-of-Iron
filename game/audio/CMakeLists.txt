add_library(audio_system STATIC
    AudioSystem.cpp
    Sound.cpp
    Music.cpp
    AudioEventHandler.cpp
)

target_include_directories(audio_system PUBLIC .)
target_link_libraries(audio_system PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Multimedia engine_core)
