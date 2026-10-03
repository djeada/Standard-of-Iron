# ---- Executable ----
qt6_add_executable(standard_of_iron
    main.cpp
    app/bootstrap/command_line.cpp
    app/bootstrap/data_paths.cpp
    app/bootstrap/log_handler.cpp
    app/bootstrap/screenshot_capture.cpp
    app/bootstrap/self_test_driver.cpp
    app/bootstrap/startup_self_test.cpp
    app/bootstrap/windows_gl_probe.cpp
    ${SOI_ASSET_STRINGS_CPP}
)

# The QML type registrations qmltyperegistrar generates include the view
# headers by their bare file name, so ui/ has to be on the include path.
target_include_directories(standard_of_iron PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# ---- QML module ----
if(QT_VERSION_MAJOR EQUAL 6)
    # Compile the complete resource collections into the executable. Listing a
    # .qrc file under qt_add_qml_module(RESOURCES) only embeds the XML file; it
    # does not register the files referenced by that collection at runtime.
    target_sources(
        standard_of_iron
        PRIVATE
            assets.qrc
            design_resources.qrc
            ${SOI_TRANSLATIONS_QRC}
            ${GENERATED_CAMPAIGN_MAP_QRC}
    )

    # qt6_add_qml_module writes the module's qmldir itself, and a `pragma
    # Singleton` file only becomes a singleton there when the source file carries
    # this property. Without it the type registers as an ordinary component and
    # every `StyleGuide.x` style reference silently evaluates to undefined.
    set_source_files_properties(
        ui/qml/StyleGuide.qml
        ui/qml/EconomyGuide.qml
        ui/qml/CameraGuide.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE
    )

    qt6_add_qml_module(standard_of_iron
        URI StandardOfIron
        VERSION 1.0
        QML_FILES
            ui/qml/Main.qml
            ui/qml/MainMenu.qml
            ui/qml/MapSelect.qml
            ui/qml/MapPreview.qml
            ui/qml/DifficultyCatalog.qml
            ui/qml/ScenarioChallenge.qml
            ui/qml/DifficultySelector.qml
            ui/qml/DifficultyBadge.qml
            ui/qml/SkirmishChip.qml
            ui/qml/MapListPanel.qml
            ui/qml/MapThumbnails.qml
            ui/qml/PlayerListItem.qml
            ui/qml/PlayerConfigPanel.qml
            ui/qml/MissionsScreen.qml
            ui/qml/MissionOrderList.qml
            ui/qml/CampaignScreen.qml
            ui/qml/MissionListItem.qml
            ui/qml/MissionDetailPanel.qml
            ui/qml/MediterraneanMapPanel.qml
            ui/qml/StyleGuide.qml
            ui/qml/StyledButton.qml
            ui/qml/StyledComboBox.qml
            ui/qml/HUD.qml
            ui/qml/HUDTop.qml
            ui/qml/MinimapOverlay.qml
            ui/qml/SystemVoice.qml
            ui/qml/CameraGuide.qml
            ui/qml/CameraLegend.qml
            ui/qml/EconomyGuide.qml
            ui/qml/AllyAppealPanel.qml
            ui/qml/EconomyCoach.qml
            ui/qml/EconomyHelpPanel.qml
            ui/qml/WaveTracker.qml
            ui/qml/MissionDeadline.qml
            ui/qml/HUDBottom.qml
            ui/qml/HUDBottomCommander.qml
            ui/qml/HUDBottomSpectator.qml
            ui/qml/UnitInspectPanel.qml
            ui/qml/BarracksProductionView.qml
            ui/qml/RecruitCard.qml
            ui/qml/RpgFpvOverlay.qml
            ui/qml/RpgMeter.qml
            ui/qml/FloatingNumbers.qml
            ui/qml/WorldProjector.qml
            ui/qml/FormationPanel.qml
            ui/qml/FormationShape.qml
            ui/qml/FormationReadoutCell.qml
            ui/qml/FormationOptionRow.qml
            ui/qml/FormationStatusBadge.qml
            ui/qml/HintCard.qml
            ui/qml/ProductionPanel.qml
            ui/qml/ProductionBarracksQueue.qml
            ui/qml/ProductionBarracksRoster.qml
            ui/qml/ProductionHomeSection.qml
            ui/qml/ProductionRallySection.qml
            ui/qml/ProductionBuilderSection.qml
            ui/qml/ProductionConstructionCard.qml
            ui/qml/ProductionMarketplaceSection.qml
            ui/qml/ProductionTempleSection.qml
            ui/qml/ProductionFarmSection.qml
            ui/qml/ProductionEmptyHint.qml
            ui/qml/SaveGamePanel.qml
            ui/qml/LoadGamePanel.qml
            ui/qml/SaveSlotRow.qml
            ui/qml/SaveProgressOverlay.qml
            ui/qml/ObjectivesPanel.qml
            ui/qml/CommanderMessagePanel.qml
            ui/qml/HelpPanel.qml
            ui/qml/TutorialOverlay.qml
            ui/qml/TutorialFocusOverlay.qml
            ui/qml/SettingsPanel.qml
            ui/qml/ControlsBindingList.qml
            ui/qml/HUDVictory.qml
            ui/qml/BattleSummary.qml
            ui/qml/GameView.qml
            ui/qml/ProfilingOverlay.qml
            ui/qml/CommanderInputLayer.qml
            ui/qml/RtsInputLayer.qml
            ui/qml/CursorLayer.qml
            ui/qml/PlayerFeedbackLayer.qml
            ui/qml/LoadScreen.qml
        RESOURCES
            ui/qml/ui_audio.js
        assets/shaders/include/environment_lighting.glsl
        assets/shaders/include/local_lighting.glsl
        assets/shaders/include/directional_shadows.glsl
        assets/shaders/include/visibility_mask.glsl
        assets/shaders/include/noise.glsl
        assets/shaders/include/foliage_bump.glsl
        assets/shaders/include/terrain_noise.glsl
        assets/shaders/directional_shadow_depth.vert
        assets/shaders/directional_shadow_depth_instanced.vert
        assets/shaders/directional_shadow_rigged.vert
        assets/shaders/directional_shadow_depth.frag
            assets/shaders/basic.frag
            assets/shaders/basic.vert
            assets/shaders/bridge.frag
            assets/shaders/bridge.vert
            assets/shaders/cylinder_instanced.frag
            assets/shaders/cylinder_instanced.vert
            assets/shaders/firecamp.frag
            assets/shaders/firecamp.vert
            assets/shaders/fog_instanced.frag
            assets/shaders/fog_instanced.vert
            assets/shaders/grass_instanced.frag
            assets/shaders/grass_instanced.vert
            assets/shaders/grid.frag
            assets/shaders/ground_plane.frag
            assets/shaders/ground_plane.vert
            assets/shaders/pine_instanced.frag
            assets/shaders/pine_instanced.vert
            assets/shaders/plant_instanced.frag
            assets/shaders/plant_instanced.vert
            assets/shaders/river.frag
            assets/shaders/river.vert
            assets/shaders/riverbank.frag
            assets/shaders/riverbank.vert
            assets/shaders/stone_instanced.frag
            assets/shaders/stone_instanced.vert
            assets/shaders/tent_instanced.frag
            assets/shaders/tent_instanced.vert
            assets/shaders/supply_cart_instanced.frag
            assets/shaders/supply_cart_instanced.vert
            assets/shaders/weapon_rack_instanced.frag
            assets/shaders/weapon_rack_instanced.vert
            assets/shaders/ruins_instanced.frag
            assets/shaders/ruins_instanced.vert
            assets/shaders/dead_tree_instanced.frag
            assets/shaders/dead_tree_instanced.vert
            assets/shaders/terrain_chunk.frag
        assets/shaders/terrain_field_bake.frag
        assets/shaders/terrain_microdetail_bake.frag
            assets/shaders/terrain_chunk.vert
            assets/maps/map_forest.json
            assets/maps/map_rivers.json
            assets/maps/map_mountain.json
            assets/maps/map_copper_canyons.json
            assets/maps/map_amber_delta.json
            assets/campaigns/second_punic_war.json
            assets/campaign_map/campaign_state.json
            assets/visuals/unit_equipment_loadouts.json
            assets/visuals/emblems/rome.png
            assets/visuals/emblems/cartaghe.png
            assets/visuals/load_screen_1.png
            assets/visuals/load_screen_2.png
            assets/visuals/load_screen_3.png
            assets/visuals/hannibal.png
            assets/visuals/scipio.png
            assets/visuals/standard_of_iron.png
            ${SOI_UNIT_ICON_RESOURCES}
        DEPENDENCIES
            Qt6::QuickControls2
    )
endif()

add_dependencies(standard_of_iron translations_qm)

target_link_libraries(
    standard_of_iron
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Sql
        $<$<TARGET_EXISTS:Qt${QT_VERSION_MAJOR}::Multimedia>:Qt${QT_VERSION_MAJOR}::Multimedia>
        ${OPENGL_LIBRARIES}
        engine_core
        render_gl
        game_systems
        audio_system
        app_core
        ui_shell
        # ui_shell is a static QML module (StandardOfIron.Core). Its plugin
        # carries the type registrations, so anything that loads QML importing
        # that module has to link it or the import fails at runtime.
        # Static QML modules keep their type registrations in a plugin; linking
        # the plugin is what puts StandardOfIron.Core, .Game and .Views into the
        # type system.
        $<$<TARGET_EXISTS:soi_qml_views>:soi_qml_views>
)

add_dependencies(
    standard_of_iron
    engine_core
    render_gl
    game_sim
    game_view
    audio_system
    app_core
)

# The version reaches the running game from exactly one place: the project()
# call above. main.cpp feeds it to QCoreApplication::applicationVersion(), and
# QML reads it back as Qt.application.version. Nothing else may spell it out --
# a hard-coded copy in a .qml or a .ts file silently outlives the tag it was
# written for.
target_compile_definitions(standard_of_iron PRIVATE SOI_VERSION="${PROJECT_VERSION}")

# Windows-specific OpenGL linking
if(WIN32)
    target_link_libraries(standard_of_iron PRIVATE opengl32)
    # Ensure we have access to wglGetProcAddress and related functions
    target_compile_definitions(standard_of_iron PRIVATE NOMINMAX)
endif()

target_link_libraries(standard_of_iron PRIVATE Qt6::QuickControls2)

set_target_properties(
    standard_of_iron
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(APPLE)
    # The macOS release workflow deploys, signs, and packages the application
    # as a bundle. Without this property CMake emits only a bare executable,
    # so macdeployqt cannot find the expected .app directory.
    set_target_properties(standard_of_iron PROPERTIES MACOSX_BUNDLE TRUE)
    # Without these, Qt's Info.plist template fills in com.yourcompany.<target>
    # and a two-part version. Notarization, Gatekeeper and Steam all key on the
    # bundle identifier, so it is fixed here once and never changed.
    # CFBundleName does not move saves: App::Core::apply_application_identity
    # pins the name QStandardPaths uses.
    set_target_properties(
        standard_of_iron
        PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.djeada.standardofiron"
            MACOSX_BUNDLE_BUNDLE_NAME "Standard of Iron"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_COPYRIGHT "Standard of Iron contributors"
    )
endif()
if(WIN32)
    set_target_properties(standard_of_iron PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
