option(
    ENABLE_GENERATED_CAMPAIGN_MAP_ASSETS
    "Embed the campaign map assets required by the shipped campaign UI"
    ON
)

# Runtime outputs from tools/map_pipeline. They are committed so that a build
# never depends on the Natural Earth or NOAA hosts; release workflows still run
# scripts/generate-campaign-map.py first to refresh them where the sources are
# reachable. The configure-time checks below prevent any build from silently
# shipping the campaign UI without them. hannibal_path.json is authored
# separately.
set(GENERATED_CAMPAIGN_MAP_RESOURCES
    assets/campaign_map/campaign_base_color.png
    assets/campaign_map/campaign_water.png
    assets/campaign_map/coastlines_uv.json
    assets/campaign_map/rivers_uv.json
    assets/campaign_map/land_mesh.bin
    assets/campaign_map/provinces.json
    assets/campaign_map/terrain_height.png
    assets/campaign_map/terrain_height.json
)

set(GENERATED_CAMPAIGN_MAP_QRC)
if(ENABLE_GENERATED_CAMPAIGN_MAP_ASSETS)
    message(STATUS "Generated campaign map assets enabled")
    set(GENERATED_CAMPAIGN_MAP_QRC "${CMAKE_CURRENT_BINARY_DIR}/campaign_map_generated.qrc")
    # Absolute paths plus an alias keep the runtime lookup at
    # ":/assets/campaign_map/..." even though the .qrc lives in the build tree.
    set(_campaign_map_qrc_entries "")
    foreach(_campaign_map_resource IN LISTS GENERATED_CAMPAIGN_MAP_RESOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_campaign_map_resource}")
            message(
                FATAL_ERROR
                "Required campaign map runtime asset is missing: ${_campaign_map_resource}"
            )
        endif()
        string(
            APPEND _campaign_map_qrc_entries
            "        <file alias=\"${_campaign_map_resource}\">${CMAKE_CURRENT_SOURCE_DIR}/${_campaign_map_resource}</file>\n"
        )
    endforeach()
    file(
        WRITE "${GENERATED_CAMPAIGN_MAP_QRC}"
        "<RCC>\n    <qresource prefix=\"/\">\n${_campaign_map_qrc_entries}    </qresource>\n</RCC>\n"
    )
else()
    message(STATUS "Generated campaign map assets disabled")
    set(GENERATED_CAMPAIGN_MAP_RESOURCES)
endif()
