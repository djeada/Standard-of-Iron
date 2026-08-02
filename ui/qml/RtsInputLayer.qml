import QtQuick 2.15
import StandardOfIron 1.0

Item {
    id: root

    property bool active: false
    property var game
    property var gameView
    property var mainWindowRef
    property var renderAreaRef

    property var pressed_keys: ({})
    property var pan_axis: ({})

    function begin_pan_action(actionId, e) {
        if (e.isAutoRepeat || pan_axis[actionId])
            return;
        pan_axis[actionId] = true;
        pressed_keys[e.key] = actionId;
        renderAreaRef.key_pan_count += 1;
        mainWindowRef.edge_scroll_disabled = true;
    }

    function any_pan_held() {
        for (var held in pan_axis) {
            if (pan_axis[held])
                return true;
        }
        return false;
    }

    function end_pan_action(actionId) {
        if (!pan_axis[actionId])
            return;
        delete pan_axis[actionId];
        renderAreaRef.key_pan_count = Math.max(0, renderAreaRef.key_pan_count - 1);
        if (!any_pan_held()) {
            if (keyPanTimer.running)
                keyPanTimer.stop();
            if (renderAreaRef.key_pan_count === 0 && !renderAreaRef.mouse_pan_active)
                mainWindowRef.edge_scroll_disabled = false;
        }
    }

    function reset_pan_keys() {
        pressed_keys = ({});
        pan_axis = ({});
        if (keyPanTimer.running)
            keyPanTimer.stop();
        if (typeof renderAreaRef !== 'undefined')
            renderAreaRef.key_pan_count = 0;
        if (typeof mainWindowRef !== 'undefined' && typeof renderAreaRef !== 'undefined' && !renderAreaRef.mouse_pan_active)
            mainWindowRef.edge_scroll_disabled = false;
    }

    function ensure_pan_timer_running() {
        if (!keyPanTimer.running)
            keyPanTimer.start();
    }

    function perform_action(actionId, event) {
        var yawStep = (event.modifiers & Qt.ShiftModifier) ? 8 : 4;
        var inputStep = (event.modifiers & Qt.ShiftModifier) ? 2 : 1;
        var shiftHeld = (event.modifiers & Qt.ShiftModifier) !== 0;
        switch (actionId) {
        case "global.toggle_control_mode":
            reset_pan_keys();
            if (root.game.toggle_commander_control_mode)
                root.game.toggle_commander_control_mode();
            return true;
        case "global.menu":
            if (typeof root.mainWindowRef === 'undefined' || root.mainWindowRef.menu_visible)
                return false;
            root.mainWindowRef.menu_visible = true;
            return true;
        case "global.quicksave":
            if (!root.game.quicksave)
                return false;
            root.game.quicksave();
            return true;
        case "global.quickload":
            if (!root.game.saves.has_save_slot || !root.game.saves.has_save_slot("quicksave"))
                return false;
            root.game.load_game_from_slot("quicksave");
            return true;
        case "rts.pause":
            if (typeof root.mainWindowRef === 'undefined')
                return false;
            root.mainWindowRef.game_paused = !root.mainWindowRef.game_paused;
            root.gameView.set_paused(root.mainWindowRef.game_paused);
            return true;
        case "rts.order_stop":
            if (!root.game.has_units_selected || !root.game.on_stop_command)
                return false;
            root.game.on_stop_command();
            return true;
        case "rts.order_attack":
            if (!root.game.has_units_selected)
                return false;
            root.game.cursor_mode = "attack";
            return true;
        case "rts.order_move":
            if (!root.game.has_units_selected)
                return false;
            root.game.cursor_mode = "normal";
            return true;
        case "rts.order_patrol":
            if (!root.game.has_units_selected)
                return false;
            root.game.cursor_mode = "patrol";
            return true;
        case "rts.order_guard":
            if (!root.game.has_units_selected)
                return false;
            root.game.cursor_mode = "guard";
            return true;
        case "rts.order_hold":
            if (!root.game.has_units_selected || !root.game.on_hold_command)
                return false;
            root.game.on_hold_command();
            return true;
        case "rts.camera_pan_up":
            begin_pan_action(actionId, event);
            root.game.camera_move(0, inputStep);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_down":
            begin_pan_action(actionId, event);
            root.game.camera_move(0, -inputStep);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_left":
            begin_pan_action(actionId, event);
            root.game.camera_move(-inputStep, 0);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_right":
            begin_pan_action(actionId, event);
            root.game.camera_move(inputStep, 0);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_yaw_left":
            root.game.camera_yaw(-yawStep);
            return true;
        case "rts.camera_yaw_right":
            root.game.camera_yaw(yawStep);
            return true;
        case "rts.camera_orbit_left":
            root.game.camera_orbit_direction(1, shiftHeld);
            return true;
        case "rts.camera_orbit_right":
            root.game.camera_orbit_direction(-1, shiftHeld);
            return true;
        case "rts.select_all_troops":
            if (!root.game.select_all_troops)
                return false;
            root.game.select_all_troops();
            return true;
        }
        return false;
    }

    function handle_key_pressed(event) {
        if (!root.active)
            return;
        if (typeof root.game === 'undefined')
            return;
        var candidates = InputBindings.actions_for_key(event.key, event.modifiers, "rts");
        for (var i = 0; i < candidates.length; ++i) {
            if (root.perform_action(candidates[i], event)) {
                event.accepted = true;
                return;
            }
        }
    }

    function handle_key_released(event) {
        if (!root.active)
            return;
        var actionId = pressed_keys[event.key];
        if (actionId === undefined)
            return;
        delete pressed_keys[event.key];
        if (actionId.indexOf("rts.camera_pan_") === 0)
            end_pan_action(actionId);
    }

    enabled: active
    focus: false
    visible: active
    Keys.enabled: active
    Component.onCompleted: {
        if (active && typeof root.gameView !== 'undefined')
            root.gameView.forceActiveFocus();
    }
    onActiveChanged: {
        if (active) {
            if (typeof root.gameView !== 'undefined')
                root.gameView.forceActiveFocus();
        } else {
            reset_pan_keys();
        }
    }

    Keys.onPressed: function (event) {
        root.handle_key_pressed(event);
    }

    Keys.onReleased: function (event) {
        root.handle_key_released(event);
    }

    Timer {
        id: keyPanTimer

        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (typeof root.game === 'undefined')
                return;
            var step = (Qt.inputModifiers & Qt.ShiftModifier) ? 2 : 1;
            var dx = 0;
            var dz = 0;
            if (root.pan_axis["rts.camera_pan_up"])
                dz += step;
            if (root.pan_axis["rts.camera_pan_down"])
                dz -= step;
            if (root.pan_axis["rts.camera_pan_left"])
                dx -= step;
            if (root.pan_axis["rts.camera_pan_right"])
                dx += step;
            if (dx !== 0 || dz !== 0)
                root.game.camera_move(dx, dz);
        }
    }
}
