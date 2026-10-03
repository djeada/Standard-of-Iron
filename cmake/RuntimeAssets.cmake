# What a player receives. `cmake --install` with no component argument
# produces exactly the game and the content it loads at runtime -- notably the
# music and ambience, which are deliberately not embedded in the executable.
install(TARGETS standard_of_iron RUNTIME DESTINATION . BUNDLE DESTINATION . COMPONENT runtime)
install(DIRECTORY assets/ DESTINATION assets COMPONENT runtime)

if(TARGET bake_creature_assets)
    add_dependencies(standard_of_iron bake_creature_assets)
endif()

# Stage assets next to the binary for dev runs.
#
# The runtime resolves ":/assets/..." against the filesystem before the Qt
# resource system, so this staged copy -- not the repo tree, and not the qrc --
# is what a dev build actually reads. It used to be a bare configure-time
# `file(COPY)`, which meant editing a shader or a map and rebuilding changed
# nothing at all until someone happened to re-run CMake. Staging on every build
# makes an edit take effect when you build, which is the only behaviour anyone
# expects.
file(
    GLOB_RECURSE SOI_RUNTIME_ASSET_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)
# Baked creature artifacts are outputs of bake_creature_assets, which writes
# them into bin/assets directly. Keeping them out of the staging glob avoids a
# dependency cycle (staging -> stamp -> baked output -> baking -> staging).
list(FILTER SOI_RUNTIME_ASSET_SOURCES EXCLUDE REGEX "assets/creatures/.*\\.(bpat|bpsm|bprm)$")
list(FILTER SOI_RUNTIME_ASSET_SOURCES EXCLUDE REGEX "assets/meshes/.*\\.(smpk|rmpk)$")
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/bin/assets/.stamp"
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/bin/assets"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/bin/assets/.stamp"
    DEPENDS ${SOI_RUNTIME_ASSET_SOURCES}
    COMMENT "Staging assets into bin/assets"
    VERBATIM
)
add_custom_target(stage_runtime_assets DEPENDS "${CMAKE_BINARY_DIR}/bin/assets/.stamp")
add_dependencies(standard_of_iron stage_runtime_assets)
if(TARGET arena_app)
    # The arena reads the staged copy too -- it is the same runtime -- so
    # without this an asset edited since the last full build is invisible to
    # every arena review and every recorded promo, silently: the tool runs, the
    # capture succeeds, and the frames show the previous version of the file.
    add_dependencies(arena_app stage_runtime_assets)
endif()
if(TARGET bake_creature_assets)
    # The baker resolves creature packages against the staged copies under
    # bin/assets before it falls back to the repo tree; staging must run
    # first or a package edited since the last full build fails its
    # integrity check and the bake aborts.
    add_dependencies(bake_creature_assets stage_runtime_assets)
endif()
