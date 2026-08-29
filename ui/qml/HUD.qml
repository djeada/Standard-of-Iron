import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import QtQuick.Window 2.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design

Item {
    id: hud

    property bool game_is_paused: false
    readonly property real current_speed: (typeof game !== 'undefined' && game && game.time_scale > 0) ? game.time_scale : 1
    property string current_command_mode: "normal"
    property int top_panel_height: topPanel.height
    property int bottom_panel_height: bottomPanel.height

    readonly property int left_stack_bottom: waveTracker.visible ? waveTracker.y + waveTracker.height : topPanel.height

    readonly property int right_stack_bottom: topPanel.height + Design.Metrics.space8 + (Design.Metrics.space24 * 8) + Design.Metrics.space8 + hudTop.minimapLegendHeight
    property int selection_tick: 0
    property bool has_movable_units: false
    property bool commander_rpg_mode: typeof game !== 'undefined' && game.commander.mode_state === "active"
    property var commander_status: ({})
    readonly property var economy: typeof game !== 'undefined' && game && game.economy ? game.economy : null
    property bool commander_rally_overlay_blocked: commander_rpg_mode && typeof game !== 'undefined' && (game.cursor_mode === "place_commander_rally" || game.cursor_mode === "place_barracks_rally")

    property bool camera_legend_visible: false

    function show_unit_profile(unit_type, nation, from_selection) {
        unitInspectPanel.show_availability = from_selection === true;
        unitInspectPanel.load(unit_type, nation);
        unitInspectPanel.visible = true;
    }

    signal pause_toggled
    signal speed_changed(real speed)
    signal camera_settings_requested
    signal command_mode_changed(string mode)
    signal recruit_unit(string unit_type)
    signal return_to_main_menu_requested
    signal campaign_requested
    signal hud_became_visible
    signal help_requested

    function refresh_command_mode() {
        var actual_mode = "normal";
        if (has_movable_units && typeof game !== 'undefined' && game.orders.command_mode)
            actual_mode = game.orders.command_mode();
        if (current_command_mode !== actual_mode)
            current_command_mode = actual_mode;
    }

    onVisibleChanged: {
        if (!visible)
            return;
        if (!Core.UiPreferences.cameraLegendSeen && !hud.commander_rpg_mode)
            hud.camera_legend_visible = true;
        hud_became_visible();
    }

    onCamera_legend_visibleChanged: {
        if (camera_legend_visible)
            Core.UiPreferences.cameraLegendSeen = true;
    }

    Connections {
        function onSelected_units_changed() {
            selection_tick += 1;
            has_movable_units = typeof game !== 'undefined' && game.orders.has_commandable_selection ? game.orders.has_commandable_selection() : false;
            refresh_command_mode();
        }

        target: (typeof game !== 'undefined') ? game : null
    }

    Timer {
        id: productionRefresh

        interval: 100
        repeat: true
        running: true
        onTriggered: {
            selection_tick += 1;
            refresh_command_mode();
        }
    }

    Timer {
        id: commanderStatusPoll

        interval: 33
        repeat: true
        running: typeof game !== 'undefined' && game.commander.mode_state === "active" && !!game.commander.status
        triggeredOnStart: true
        onTriggered: hud.commander_status = game.commander.status()
    }

    QtObject {
        id: fpsMeter

        property int frames: 0
        property real fps: 0
    }

    Connections {
        target: Window.window ? Window.window : null
        function onFrameSwapped() {
            fpsMeter.frames += 1;
        }
    }

    Timer {
        id: fpsPoll

        interval: 500
        repeat: true
        running: visible && Core.UiPreferences.showFps && Window.window !== null
        triggeredOnStart: true
        onTriggered: {
            fpsMeter.fps = Math.round(fpsMeter.frames * (1000 / interval));
            fpsMeter.frames = 0;
            fpsReadout.text = qsTr("%1 FPS").arg(fpsMeter.fps);
        }
    }

    Text {
        id: fpsReadout

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        anchors.bottomMargin: hud.bottom_panel_height + Design.Metrics.space8
        visible: Core.UiPreferences.showFps
        text: ""
        color: Design.Theme.textSecondary
        font.family: "monospace"
        font.pixelSize: Design.Typography.caption
    }

    Item {
        id: topPanel

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(50, parent.height * 0.08)

        HUDTop {
            id: hudTop

            anchors.fill: parent
            game_is_paused: hud.game_is_paused
            current_speed: hud.current_speed
            onPause_toggled: {
                hud.game_is_paused = !hud.game_is_paused;
                hud.pause_toggled();
            }
            onSpeed_changed: function (s) {
                hud.speed_changed(s);
            }
            onEconomy_help_requested: economyHelpPanel.visible = true
            onHelp_requested: hud.help_requested()
            camera_legend_visible: hud.camera_legend_visible
            onCamera_legend_toggled: hud.camera_legend_visible = !hud.camera_legend_visible
        }
    }

    Item {
        id: bottomPanel

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        height: Design.Metrics.bottomBarHeight(parent.height, hud.commander_rpg_mode)
        clip: true

        Loader {
            id: bottomPanelLoader
            anchors.fill: parent
            sourceComponent: {
                if (typeof game === 'undefined')
                    return rtsBottomHudComponent;
                if (game.is_spectator_mode)
                    return spectatorBottomHudComponent;
                return game.commander.mode_state === "active" ? commanderBottomHudComponent : rtsBottomHudComponent;
            }
        }

        Component {
            id: spectatorBottomHudComponent

            HUDBottomSpectator {
                objectName: "spectatorBottomHud"
                anchors.fill: parent
                selection_tick: hud.selection_tick
                onFollow_requested: function (owner_id) {
                    if (typeof game !== 'undefined')
                        game.selected_player_id = owner_id;
                }
            }
        }

        Component {
            id: rtsBottomHudComponent

            HUDBottom {
                anchors.fill: parent
                current_command_mode: hud.current_command_mode
                selection_tick: hud.selection_tick
                has_movable_units: hud.has_movable_units
                onCommand_mode_changed: function (m) {
                    hud.current_command_mode = m;
                    hud.command_mode_changed(m);
                }
                onRecruit_unit: function (unit_type) {
                    hud.recruit_unit(unit_type);
                }
                onUnit_profile_requested: function (unit_type, nation, from_selection) {
                    hud.show_unit_profile(unit_type, nation, from_selection);
                }
            }
        }

        Component {
            id: commanderBottomHudComponent

            HUDBottomCommander {
                anchors.fill: parent
                external_status: hud.commander_status
            }
        }
    }

    WaveTracker {
        id: waveTracker

        anchors.top: topPanel.bottom
        anchors.left: parent.left
        anchors.topMargin: Design.Metrics.space8
        anchors.leftMargin: Design.Metrics.hudZoneMargin

        visible: has_waves && !hud.commander_rpg_mode && !(typeof game !== 'undefined' && game.tutorial && game.tutorial.holds_mission_clock)
    }

    EconomyCoach {
        id: economyCoach

        anchors.top: topPanel.bottom
        anchors.left: parent.left
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        anchors.topMargin: Design.Metrics.space8 + (waveTracker.visible ? waveTracker.height + Design.Metrics.space8 : 0)

        economy: hud.economy
        visible: !hud.commander_rpg_mode && !!hud.economy && hud.economy.coach_visible
        onHelp_requested: economyHelpPanel.visible = true
    }

    CameraLegend {
        id: cameraLegend

        anchors.right: parent.right
        anchors.rightMargin: Design.Metrics.hudZoneMargin
        anchors.top: topPanel.bottom
        anchors.topMargin: hud.right_stack_bottom - topPanel.height

        visible: hud.camera_legend_visible && !hud.commander_rpg_mode && !commanderMessage.showing
        onDismissed: hud.camera_legend_visible = false
        onOpen_settings_requested: {
            hud.camera_legend_visible = false;
            hud.camera_settings_requested();
        }
    }

    UnitInspectPanel {
        id: unitInspectPanel

        anchors.fill: parent
        visible: false
        onVisibleChanged: {
            if (visible) {
                unitInspectPanel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
        }
        onClose_requested: unitInspectPanel.visible = false
    }

    EconomyHelpPanel {
        id: economyHelpPanel

        anchors.fill: parent
        economy: hud.economy
        visible: false
        onVisibleChanged: {
            if (visible) {
                economyHelpPanel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
        }
        onClose_requested: economyHelpPanel.visible = false
    }

    FormationPanel {
        id: formationPanel

        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16

        max_height: Math.max(0, bottomPanel.y - (waveTracker.y + waveTracker.height) - Design.Metrics.space16)
        placing: typeof game !== 'undefined' && game.placement !== undefined && game.placement.is_placing_formation
        visible: placing && !hud.commander_rpg_mode
    }

    FormationStatusBadge {
        id: formationStatusBadge

        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        visible: has_formation && !formationPanel.placing && !hud.commander_rpg_mode
    }

    WorldProjector {
        id: worldProjector

        anchors.fill: parent
        camera: typeof game !== 'undefined' ? game.camera : null
        topInset: topPanel.height
        bottomInset: bottomPanel.height
        active: floatingNumbers.visible || rpgFpvOverlay.visible || tutorialFocusOverlay.visible
    }

    RpgFpvOverlay {
        id: rpgFpvOverlay
        anchors.fill: parent
        bottomInset: bottomPanel.height
        topInset: topPanel.height
        status: hud.commander_status
        projector: worldProjector
        visible: hud.commander_rpg_mode && !hud.commander_rally_overlay_blocked
    }

    TutorialFocusOverlay {
        id: tutorialFocusOverlay

        anchors.fill: parent
        projector: worldProjector
        points: (typeof game !== 'undefined' && game.tutorial && game.tutorial.active) ? game.tutorial.focus_points : []
        topInset: topPanel.height
        bottomInset: bottomPanel.height
        z: 1
        visible: !hud.commander_rpg_mode && points.length > 0
    }

    FloatingNumbers {
        id: floatingNumbers

        anchors.fill: parent
        source: typeof game !== 'undefined' ? game.activity : null
        projector: worldProjector
        combatEnabled: Design.A11y.damageNumbers
        economyEnabled: Design.A11y.economyNumbers && !hud.commander_rpg_mode
        visible: !hud.commander_rally_overlay_blocked
    }

    CommanderMessagePanel {
        id: commanderMessage

        anchors.right: parent.right
        anchors.rightMargin: Design.Metrics.hudZoneMargin
        anchors.top: topPanel.bottom
        anchors.topMargin: hud.right_stack_bottom - topPanel.height

        z: 200
    }

    HUDVictory {
        id: hudVictory

        anchors.fill: parent
        onReturn_to_main_menu_requested: {
            hud.return_to_main_menu_requested();
        }
        onCampaign_requested: {
            hud.campaign_requested();
        }

        Connections {
            function onHud_became_visible() {
                if (typeof game !== 'undefined' && game.victory_state === "")
                    hudVictory.force_hide();
            }

            target: hud
        }
    }
}
