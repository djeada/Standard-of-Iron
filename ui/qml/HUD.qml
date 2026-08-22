import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
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
            var has_troops = false;
            if (typeof game !== 'undefined' && game.has_units_selected && game.production.has_selected_type) {
                var troop_types = ["warrior", "archer", "swordsman", "spearman", "healer", "catapult", "ballista", "horse_archer", "horse_swordsman", "horse_spearman", "elephant", "builder", "civilian"];
                for (var i = 0; i < troop_types.length; i++) {
                    if (game.production.has_selected_type(troop_types[i])) {
                        has_troops = true;
                        break;
                    }
                }
            }
            has_movable_units = has_troops;
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

        height: hud.commander_rpg_mode ? Math.max(96, Math.min(120, parent.height * 0.12)) : Math.max(216, parent.height * 0.24)
        clip: true

        Loader {
            id: bottomPanelLoader
            anchors.fill: parent
            sourceComponent: typeof game !== 'undefined' && game.commander.mode_state === "active" ? commanderBottomHudComponent : rtsBottomHudComponent
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
        anchors.topMargin: Design.Metrics.space8 + (Design.Metrics.space24 * 8) + Design.Metrics.space8 + hudTop.minimapLegendHeight

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

    RpgFpvOverlay {
        id: rpgFpvOverlay
        anchors.fill: parent
        bottomInset: bottomPanel.height
        topInset: topPanel.height
        status: hud.commander_status
        camera: typeof game !== 'undefined' ? game.camera : null
        visible: hud.commander_rpg_mode && !hud.commander_rally_overlay_blocked
    }

    RpgDamageNumbers {
        id: rpgDamageNumbers
        anchors.fill: parent
        commander: typeof game !== 'undefined' ? game.commander : null
        camera: typeof game !== 'undefined' ? game.camera : null

        visible: hud.commander_rpg_mode && !hud.commander_rally_overlay_blocked && Design.A11y.damageNumbers
    }

    TutorialFocusOverlay {
        id: tutorialFocusOverlay

        anchors.fill: parent
        camera: typeof game !== 'undefined' ? game.camera : null
        points: (typeof game !== 'undefined' && game.tutorial && game.tutorial.active) ? game.tutorial.focus_points : []
        topInset: topPanel.height
        bottomInset: bottomPanel.height
        z: 1
        visible: !hud.commander_rpg_mode && points.length > 0
    }

    CombatDamageNumbers {
        id: combatDamageNumbers

        anchors.fill: parent
        activitySource: typeof game !== 'undefined' ? game.activity : null
        camera: typeof game !== 'undefined' ? game.camera : null
        visible: !hud.commander_rpg_mode && Design.A11y.damageNumbers
    }

    CommanderMessagePanel {
        id: commanderMessage

        anchors.right: parent.right
        anchors.rightMargin: Design.Metrics.hudZoneMargin
        anchors.top: topPanel.bottom
        anchors.topMargin: Design.Metrics.space8 + (Design.Metrics.space24 * 8) + Design.Metrics.space8 + hudTop.minimapLegendHeight

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
