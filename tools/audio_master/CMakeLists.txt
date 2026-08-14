add_executable(audio_master_preview main.cpp)

# The mastering chain is linked, not recompiled: this tool exists to audition
# what the game will sound like, which only holds if it runs the same objects.
target_link_libraries(audio_master_preview PRIVATE soi_audio_mastering)

target_include_directories(audio_master_preview PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(audio_master_preview SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

set_target_properties(
    audio_master_preview
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
