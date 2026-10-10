# ---- campaign_map_film: the campaign map as a film set ----------------------
#
# Films the game's own Mediterranean map (CampaignMapView, the item the
# campaign screen shows) with no UI chrome, from a camera/route spec. See
# docs/CAMPAIGN_MAP_FILM.md.
#
# The spec parser, the camera and route timeline and the overlay painter need
# no GL, so they are a library: tools_tests links the code the tool ships.
add_library(campaign_film_core STATIC campaign_film_spec.cpp campaign_film_overlay.cpp)
target_include_directories(campaign_film_core PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(
    campaign_film_core
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
)

if(QT_VERSION_MAJOR EQUAL 6)
    add_executable(campaign_map_film main.cpp)
    target_link_libraries(
        campaign_map_film
        PRIVATE
            campaign_film_core
            soi_qml_views
            ui_shell
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::OpenGL
    )
    set_target_properties(
        campaign_map_film
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
