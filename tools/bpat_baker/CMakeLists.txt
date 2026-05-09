add_executable(bpat_baker
    main.cpp
)

target_link_libraries(bpat_baker
    PRIVATE
        render_gl
        engine_core
        game_systems
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(bpat_baker
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

set_target_properties(bpat_baker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(_soi_creature_assets_source_dir "${CMAKE_SOURCE_DIR}/assets/creatures")
set(_soi_creature_assets_build_dir "${CMAKE_BINARY_DIR}/bin/assets/creatures")

add_custom_target(bake_creature_assets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_soi_creature_assets_source_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_soi_creature_assets_build_dir}"
    COMMAND "$<TARGET_FILE:bpat_baker>" "${_soi_creature_assets_source_dir}"
    COMMAND "$<TARGET_FILE:bpat_baker>" "${_soi_creature_assets_build_dir}"
    DEPENDS bpat_baker
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Baking creature animation assets"
    VERBATIM
)
