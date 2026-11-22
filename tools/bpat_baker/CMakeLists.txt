add_executable(bpat_baker main.cpp)

target_link_libraries(
    bpat_baker
    PRIVATE
        creature_bake
        render_gl
        engine_core
        game_sim
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(bpat_baker PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(bpat_baker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(SOI_RELEASE_ASSET_COMPRESSION "Use smaller baked assets for release packaging" OFF)
set(_soi_bake_profile development)
if(SOI_RELEASE_ASSET_COMPRESSION)
    set(_soi_bake_profile release)
endif()

# Make also needs a file dependency when only the command-line profile changes.
set(_soi_bake_profile_file "${CMAKE_CURRENT_BINARY_DIR}/bake_profile.txt")
file(CONFIGURE OUTPUT "${_soi_bake_profile_file}" CONTENT "${_soi_bake_profile}\n")

# The attachment baker loads the creature and building files from bin/assets.
# Keep metadata outside that runtime tree so asset copies contain only frames.
set(_soi_bake_metadata_dir "${CMAKE_BINARY_DIR}/baked-asset-metadata")

set(_soi_creature_assets_source_dir "${CMAKE_SOURCE_DIR}/assets/creatures")
set(_soi_creature_assets_build_dir "${CMAKE_BINARY_DIR}/bin/assets/creatures")

set(_soi_creature_asset_names
    humanoid.bpat
    humanoid_sword.bpat
    humanoid_spear.bpat
    humanoid_skeleton.bpat
    humanoid_caster.bpat
    humanoid_stave_caster.bpat
    horse.bpat
    horse_minimal.bpsm
    elephant.bpat
    elephant_minimal.bpsm
    sheep.bpat
    sheep_minimal.bpsm
    wolf.bpat
    wolf_minimal.bpsm
    # Bodies, baked from the species' C++ part graphs. Named after the graph, so
    # the humanoid stances that share one share a file.
    humanoid_full.bprm
    humanoid_minimal.bprm
    # Bearded humanoid bodies: the beard is part of the body graph.
    humanoid_short_beard_full.bprm
    humanoid_short_beard_minimal.bprm
    humanoid_full_beard_full.bprm
    humanoid_full_beard_minimal.bprm
    humanoid_long_goatee_full.bprm
    humanoid_long_goatee_minimal.bprm
    humanoid_mustache_beard_full.bprm
    humanoid_mustache_beard_minimal.bprm
    skeleton_humanoid_full.bprm
    skeleton_humanoid_minimal.bprm
    horse_full.bprm
    horse_minimal.bprm
    elephant_full.bprm
    elephant_minimal.bprm
    sheep_full.bprm
    sheep_minimal.bprm
    wolf_full.bprm
    wolf_minimal.bprm
)

# Static meshes merged from the procedural building catalog, one pack per family.
set(_soi_mesh_assets_source_dir "${CMAKE_SOURCE_DIR}/assets/meshes")
set(_soi_mesh_assets_build_dir "${CMAKE_BINARY_DIR}/bin/assets/meshes")
set(_soi_mesh_asset_names buildings.smpk attachments.rmpk)

# Bake in the runtime tree and copy into the source tree for tests run there.
set(_soi_asset_outputs)
set(_soi_asset_sidecars)
set(_soi_creature_asset_baked)
foreach(_asset_name IN LISTS _soi_creature_asset_names)
    list(APPEND _soi_creature_asset_baked "${_soi_creature_assets_build_dir}/${_asset_name}")
    list(APPEND _soi_asset_sidecars "${_soi_bake_metadata_dir}/${_asset_name}.sha256")
    list(APPEND _soi_asset_outputs "${_soi_creature_assets_source_dir}/${_asset_name}")
endforeach()
set(_soi_mesh_asset_baked)
foreach(_asset_name IN LISTS _soi_mesh_asset_names)
    list(APPEND _soi_mesh_asset_baked "${_soi_mesh_assets_build_dir}/${_asset_name}")
    list(APPEND _soi_asset_sidecars "${_soi_bake_metadata_dir}/${_asset_name}.sha256")
    list(APPEND _soi_asset_outputs "${_soi_mesh_assets_source_dir}/${_asset_name}")
endforeach()

include(${CMAKE_SOURCE_DIR}/cmake/CreatureBakeInputs.cmake)

# Make checks the stamp even when the baker reuses every frame. Ninja restats
# the individual frame outputs and skips the copy when they are all unchanged.
set(_soi_bake_stamp "${CMAKE_CURRENT_BINARY_DIR}/bake.stamp")
add_custom_command(
    OUTPUT
        "${_soi_bake_stamp}"
        ${_soi_creature_asset_baked}
        ${_soi_mesh_asset_baked}
        ${_soi_asset_sidecars}
    COMMAND
        "${CMAKE_COMMAND}" -E make_directory "${_soi_creature_assets_build_dir}"
        "${_soi_mesh_assets_build_dir}" "${_soi_bake_metadata_dir}"
    COMMAND
        "$<TARGET_FILE:bpat_baker>" "${_soi_creature_assets_build_dir}"
        "${_soi_mesh_assets_build_dir}" "${_soi_bake_profile}" "${_soi_bake_metadata_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_soi_bake_stamp}"
    DEPENDS bpat_baker ${SOI_CREATURE_BAKE_INPUTS} "${_soi_bake_profile_file}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Baking creature animation and static mesh assets"
    VERBATIM
)

set(_soi_copy_stamp "${CMAKE_CURRENT_BINARY_DIR}/copy.stamp")
add_custom_command(
    OUTPUT "${_soi_copy_stamp}" ${_soi_asset_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_soi_creature_assets_source_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_soi_mesh_assets_source_dir}"
    COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different ${_soi_creature_asset_baked}
        "${_soi_creature_assets_source_dir}"
    COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different ${_soi_mesh_asset_baked}
        "${_soi_mesh_assets_source_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_soi_copy_stamp}"
    DEPENDS ${_soi_creature_asset_baked} ${_soi_mesh_asset_baked}
    COMMENT "Staging baked creature animation and static mesh assets"
    VERBATIM
)

add_custom_target(bake_creature_assets DEPENDS "${_soi_copy_stamp}" ${_soi_asset_outputs})
