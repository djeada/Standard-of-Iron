import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: hud

    property bool game_is_paused: false
    property real current_speed: 1
    property string current_command_mode: "normal"
    property int top_panel_height: topPanel.height
    property int bottom_panel_height: bottomPanel.height
    property int selection_tick: 0
    property bool has_movable_units: false

    signal pause_toggled
    signal speed_changed(real speed)
    signal command_mode_changed(string mode)
    signal recruit_unit(string unit_type)
    signal return_to_main_menu_requested
    signal hud_became_visible

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
            var actual_mode = "normal";
            if (has_troops && typeof game !== 'undefined' && game.cursor_mode && game.cursor_mode !== "normal")
                actual_mode = game.cursor_mode;
            else if (has_troops && typeof game !== 'undefined' && game.get_selected_units_command_mode)
                actual_mode = game.get_selected_units_command_mode();
            if (current_command_mode !== actual_mode) {
                current_command_mode = actual_mode;
                command_mode_changed(actual_mode);
            }
            has_movable_units = has_troops;
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
            if (has_movable_units && typeof game !== 'undefined' && game.get_selected_units_command_mode) {
                var actual_mode = (game.cursor_mode && game.cursor_mode !== "normal") ? game.cursor_mode : game.get_selected_units_command_mode();
                if (current_command_mode !== actual_mode)
                    current_command_mode = actual_mode;
            }
        }
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
        height: Math.max(140, parent.height * 0.2)

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
            }
        }
    }

    RpgTargetBar {
        id: rpgTargetBar

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: topPanel.bottom
        anchors.topMargin: 8
        visible: typeof game !== 'undefined' && game.game_mode === "rpg" && target_max_hp > 0

        property var _status: typeof game !== 'undefined' && game.get_controlled_commander_status ? game.get_controlled_commander_status() : null

        target_name: _status ? (_status["locked_target_name"] || "") : ""
        target_hp: _status ? (_status["locked_target_hp"] || 0) : 0
        target_max_hp: _status ? (_status["locked_target_max_hp"] || 0) : 0
        target_hp_ratio: _status ? (_status["locked_target_hp_ratio"] || 0.0) : 0.0

        Timer {
            interval: 100
            repeat: true
            running: true
            onTriggered: {
                if (typeof game !== 'undefined' && game.game_mode === "rpg" && game.get_controlled_commander_status) {
                    var s = game.get_controlled_commander_status();
                    rpgTargetBar.target_name = s["locked_target_name"] || "";
                    rpgTargetBar.target_hp = s["locked_target_hp"] || 0;
                    rpgTargetBar.target_max_hp = s["locked_target_max_hp"] || 0;
                    rpgTargetBar.target_hp_ratio = s["locked_target_hp_ratio"] || 0.0;
                }
            }
        }
    }

    RpgFpvOverlay {
        id: rpgFpvOverlay
        anchors.fill: parent
        visible: typeof game !== 'undefined' && game.control_mode === "commander"
    }

    RpgDamageNumbers {
        id: rpgDamageNumbers
        anchors.fill: parent
        engine: typeof game !== 'undefined' ? game : null
        visible: typeof game !== 'undefined' && game.game_mode === "rpg" && game.control_mode === "commander"
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
