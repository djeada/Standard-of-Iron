# The baker reads source creature packages and the content used to construct
# nation-specific attachment meshes. Baked outputs must never enter this list.
file(
    GLOB_RECURSE SOI_CREATURE_BAKE_INPUTS
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/creatures/*.cmesh"
    "${CMAKE_SOURCE_DIR}/assets/data/troops/*.json"
    "${CMAKE_SOURCE_DIR}/assets/data/nations/*.json"
    "${CMAKE_SOURCE_DIR}/assets/data/formations/*.json"
    "${CMAKE_SOURCE_DIR}/assets/data/construction/*.json"
    "${CMAKE_SOURCE_DIR}/assets/visuals/unit_equipment_loadouts.json"
)

# Removing an input also changes the baked result. Snapshot the list so removals
# invalidate the bake even when all remaining inputs are older than its outputs.
set(_soi_creature_bake_input_list "${CMAKE_CURRENT_BINARY_DIR}/creature_bake_inputs.txt")
file(
    CONFIGURE
    OUTPUT "${_soi_creature_bake_input_list}"
    CONTENT "${SOI_CREATURE_BAKE_INPUTS}\n"
    @ONLY
)
list(APPEND SOI_CREATURE_BAKE_INPUTS "${_soi_creature_bake_input_list}")
