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
# The baker updates these files in bin/assets. Exclude them from the copy so
# source-tree copies cannot overwrite a cache restored for this build.
set(_soi_creature_bake_outputs "assets/creatures/.*[.](bpat|bpsm|bprm)$")
set(_soi_mesh_bake_outputs "assets/meshes/.*[.](smpk|rmpk)$")
list(FILTER SOI_RUNTIME_ASSET_SOURCES EXCLUDE REGEX "${_soi_creature_bake_outputs}")
list(FILTER SOI_RUNTIME_ASSET_SOURCES EXCLUDE REGEX "${_soi_mesh_bake_outputs}")
set(_soi_stage_assets_script "${CMAKE_CURRENT_BINARY_DIR}/stage_runtime_assets.cmake")
file(
    CONFIGURE
    OUTPUT "${_soi_stage_assets_script}"
    CONTENT
        [=[
file(COPY "@CMAKE_CURRENT_SOURCE_DIR@/assets/"
    DESTINATION "@CMAKE_BINARY_DIR@/bin/assets"
    REGEX "@_soi_creature_bake_outputs@" EXCLUDE
    REGEX "@_soi_mesh_bake_outputs@" EXCLUDE)
]=]
    @ONLY
)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/bin/assets/.stamp"
    COMMAND ${CMAKE_COMMAND} -P "${_soi_stage_assets_script}"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/bin/assets/.stamp"
    DEPENDS ${SOI_RUNTIME_ASSET_SOURCES} "${_soi_stage_assets_script}"
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
if(TARGET campaign_map_film)
    # The map film reads the campaign map, its shaders and the march from the
    # staged copy, like the game; an unstaged edit would film the old file.
    add_dependencies(campaign_map_film stage_runtime_assets)
endif()
if(TARGET bake_creature_assets)
    # The baker resolves creature packages against the staged copies under
    # bin/assets before it falls back to the repo tree; staging must run
    # first or a package edited since the last full build fails its
    # integrity check and the bake aborts.
    add_dependencies(bake_creature_assets stage_runtime_assets)
endif()
