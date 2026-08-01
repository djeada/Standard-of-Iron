add_executable(bpat_baker main.cpp)

target_link_libraries(
    bpat_baker
    PRIVATE
        render_gl
        engine_core
        game_systems
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(bpat_baker PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(bpat_baker PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
    # Bodies, baked from the species' C++ part graphs. Named after the graph, so
    # the humanoid stances that share one share a file.
    humanoid_full.bprm
    humanoid_minimal.bprm
    skeleton_humanoid_full.bprm
    skeleton_humanoid_minimal.bprm
    horse_full.bprm
    horse_minimal.bprm
    elephant_full.bprm
    elephant_minimal.bprm
)

set(_soi_creature_asset_outputs)
foreach(_asset_name IN LISTS _soi_creature_asset_names)
    list(
        APPEND _soi_creature_asset_outputs
        "${_soi_creature_assets_source_dir}/${_asset_name}"
        "${_soi_creature_assets_build_dir}/${_asset_name}"
    )
endforeach()

add_custom_command(
    OUTPUT ${_soi_creature_asset_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_soi_creature_assets_source_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_soi_creature_assets_build_dir}"
    COMMAND "$<TARGET_FILE:bpat_baker>" "${_soi_creature_assets_source_dir}"
    COMMAND "$<TARGET_FILE:bpat_baker>" "${_soi_creature_assets_build_dir}"
    DEPENDS bpat_baker
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Baking creature animation assets"
    VERBATIM
)

add_custom_target(bake_creature_assets DEPENDS ${_soi_creature_asset_outputs})
