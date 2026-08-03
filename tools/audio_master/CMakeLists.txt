add_executable(audio_master_preview main.cpp ${CMAKE_SOURCE_DIR}/game/audio/audio_mastering.cpp)

target_include_directories(audio_master_preview PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(audio_master_preview SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

set_target_properties(
    audio_master_preview
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
