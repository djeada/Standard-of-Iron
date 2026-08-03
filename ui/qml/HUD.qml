import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: hud

    property bool game_is_paused: false
    property real current_speed: 1
    property string current_command_mode: "normal"
    property int top_panel_height: topPanel.height
    property int bottom_panel_height: bottomPanel.height
    property int selection_tick: 0
    property bool has_movable_units: false
    property bool commander_rpg_mode: typeof game !== 'undefined' && game.control_mode === "commander" && game.game_mode === "rpg"
    property var commander_status: ({})
    property bool commander_rally_overlay_blocked: commander_rpg_mode && typeof game !== 'undefined' && (game.cursor_mode === "place_commander_rally" || game.cursor_mode === "place_barracks_rally")

    signal pause_toggled
    signal speed_changed(real speed)
    signal command_mode_changed(string mode)
    signal recruit_unit(string unit_type)
    signal return_to_main_menu_requested
    signal hud_became_visible

    function refresh_command_mode() {
        var actual_mode = "normal";
        if (has_movable_units && typeof game !== 'undefined' && game.get_selected_units_command_mode)
            actual_mode = game.get_selected_units_command_mode();
        if (current_command_mode !== actual_mode)
            current_command_mode = actual_mode;
    }

    onVisibleChanged: {
        if (visible)
            hud_became_visible();
    }

    Connections {
        function onSelected_units_changed() {
            selection_tick += 1;
            var has_troops = false;
            if (typeof game !== 'undefined' && game.has_units_selected && game.has_selected_type) {
                var troop_types = ["warrior", "archer", "swordsman", "spearman", "healer", "catapult", "ballista", "horse_archer", "horse_swordsman", "horse_spearman", "elephant", "builder", "civilian"];
                for (var i = 0; i < troop_types.length; i++) {
                    if (game.has_selected_type(troop_types[i])) {
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
        running: typeof game !== 'undefined' && game.control_mode === "commander" && !!game.get_controlled_commander_status
        triggeredOnStart: true
        onTriggered: hud.commander_status = game.get_controlled_commander_status()
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
                hud.current_speed = s;
                hud.speed_changed(s);
            }
        }
    }

    Item {
        id: bottomPanel

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        height: hud.commander_rpg_mode ? Math.max(132, Math.min(158, parent.height * 0.16)) : Math.max(216, parent.height * 0.24)
        clip: true

        Loader {
            id: bottomPanelLoader
            anchors.fill: parent
            sourceComponent: typeof game !== 'undefined' && game.control_mode === "commander" ? commanderBottomHudComponent : rtsBottomHudComponent
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
    }

    FormationPanel {
        id: formationPanel

        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        placing: typeof game !== 'undefined' && game.placement !== undefined && game.placement.is_placing_formation
    }

    FormationStatusBadge {
        id: formationStatusBadge

        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        visible: has_formation && !formationPanel.placing
    }

    RpgTargetBar {
        id: rpgTargetBar

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: topPanel.bottom
        anchors.topMargin: 8
        visible: hud.commander_rpg_mode && target_max_hp > 0

        readonly property var _status: hud.commander_status

        target_name: _status ? (_status["locked_target_name"] || "") : ""
        target_hp: _status ? (_status["locked_target_hp"] || 0) : 0
        target_max_hp: _status ? (_status["locked_target_max_hp"] || 0) : 0
        target_hp_ratio: _status ? (_status["locked_target_hp_ratio"] || 0.0) : 0.0
        target_staggered: _status ? !!_status["locked_target_staggered"] : false
        target_guard_broken: _status ? !!_status["locked_target_guard_broken"] : false
    }

    RpgFpvOverlay {
        id: rpgFpvOverlay
        anchors.fill: parent
        bottomInset: bottomPanel.height
        status: hud.commander_status
        engine: typeof game !== 'undefined' ? game : null
        visible: hud.commander_rpg_mode && !hud.commander_rally_overlay_blocked
    }

    RpgDamageNumbers {
        id: rpgDamageNumbers
        anchors.fill: parent
        engine: typeof game !== 'undefined' ? game : null

        visible: hud.commander_rpg_mode && !hud.commander_rally_overlay_blocked && Design.A11y.damageNumbers
    }

    HUDVictory {
        id: hudVictory

        anchors.fill: parent
        onReturn_to_main_menu_requested: {
            hud.return_to_main_menu_requested();
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
