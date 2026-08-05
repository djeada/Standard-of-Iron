# The looping ambience beds are rendered from the recipes in this directory
# rather than being committed, for the same reason the creature clips are baked:
# the recipe is the source of truth and the .ogg is output. See
# docs/AUDIO_MASTERING.md.
#
# Rendering needs Python and an ffmpeg that can write Vorbis, because
# assets/audio/audio_manifest.json pins each bed to an .ogg path. When either is
# missing the build says so and carries on: the resource loader already treats a
# bed it cannot open as "play nothing", so the game runs without ambience rather
# than failing to build on a machine that cannot render it.

find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(SOI_FFMPEG_EXECUTABLE ffmpeg)

set(_soi_ambience_script "${CMAKE_CURRENT_SOURCE_DIR}/synthesize_ambience.py")
set(_soi_ambience_recipes "${CMAKE_CURRENT_SOURCE_DIR}/ambience.py")
set(_soi_ambience_dsp "${CMAKE_CURRENT_SOURCE_DIR}/dsp.py")

set(_soi_ambience_source_dir "${CMAKE_SOURCE_DIR}/assets/audio/ambience")
set(_soi_ambience_build_dir "${CMAKE_BINARY_DIR}/bin/assets/audio/ambience")
set(_soi_ambience_stamp "${CMAKE_BINARY_DIR}/ambience_beds.stamp")

if(Python3_Interpreter_FOUND AND SOI_FFMPEG_EXECUTABLE)
    # Rendered into the source tree and into the staged copy, the way
    # bake_creature_assets does, so neither ordering against
    # stage_runtime_assets can leave the beds missing.
    add_custom_command(
        OUTPUT "${_soi_ambience_stamp}"
        COMMAND
            "${Python3_EXECUTABLE}" "${_soi_ambience_script}" --out "${_soi_ambience_source_dir}"
        COMMAND
            "${CMAKE_COMMAND}" -E copy_directory "${_soi_ambience_source_dir}"
            "${_soi_ambience_build_dir}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${_soi_ambience_stamp}"
        DEPENDS "${_soi_ambience_script}" "${_soi_ambience_recipes}" "${_soi_ambience_dsp}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Rendering ambience beds"
        VERBATIM
    )
    add_custom_target(synthesize_ambience_assets DEPENDS "${_soi_ambience_stamp}")
else()
    if(NOT Python3_Interpreter_FOUND)
        message(
            WARNING
            "Python 3 not found: the ambience beds cannot be rendered and the game will run without ambience."
        )
    else()
        message(
            WARNING
            "ffmpeg not found: the ambience beds cannot be rendered and the game will run without ambience."
        )
    endif()
    add_custom_target(synthesize_ambience_assets)
endif()
