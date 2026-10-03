# ---- QML design system regression tests ----
# QtQuickTest drives the real StandardOfIron.Design module, so token drift or a
# renamed control fails here rather than in a screen. Skipped when the Qt Quick
# test module is unavailable so a minimal Qt install still builds the suite.
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS QuickTest)
if(TARGET Qt${QT_VERSION_MAJOR}::QuickTest)
    # qt_add_executable, not add_executable: it runs the finalizer that imports
    # the static QML plugins the app modules ship, which is what lets this binary
    # resolve StandardOfIron.Core, .Game and .Views the way the game does.
    qt_add_executable(
        design_system_qml_tests
        ui/design_system_qml_test.cpp
        ${CMAKE_SOURCE_DIR}/design_resources.qrc
        ${CMAKE_SOURCE_DIR}/assets.qrc
    )
    target_link_libraries(
        design_system_qml_tests
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Qml
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::QuickTest
            # The app QML this binary instantiates -- the spectator HUD, the
            # inspector, the commander message panel -- imports
            # StandardOfIron.Core and StandardOfIron.Views, so the libraries
            # backing those modules have to be here for the import to resolve.
            ui_shell
            $<$<TARGET_EXISTS:soi_qml_views>:soi_qml_views>
    )
    target_include_directories(design_system_qml_tests PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(
        design_system_qml_tests
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" AUTOMOC ON AUTORCC ON
    )
    add_test(
        NAME design_system_qml
        COMMAND design_system_qml_tests -input ${CMAKE_CURRENT_SOURCE_DIR}/ui/qml
    )
    # The design system is presentation-only; a real GPU adds nothing to assert.
    set_tests_properties(design_system_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
