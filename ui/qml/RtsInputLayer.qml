import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Core 1.0

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
        }
    }

    function reset_pan_keys() {
        pressed_keys = ({});
        pan_axis = ({});
        if (keyPanTimer.running)
            keyPanTimer.stop();
        if (typeof renderAreaRef !== 'undefined')
            renderAreaRef.key_pan_count = 0;
    }

    function ensure_pan_timer_running() {
        if (!keyPanTimer.running)
            keyPanTimer.start();
    }

    function perform_action(actionId, event) {
        var shiftHeld = (event.modifiers & Qt.ShiftModifier) !== 0;
        var yawStep = shiftHeld ? 8 : 4;
        var inputStep = shiftHeld ? 2 : 1;
        var zoomStep = shiftHeld ? 2 : 1;
        switch (actionId) {
        case "global.toggle_control_mode":
            reset_pan_keys();
            if (root.game.commander.toggle_mode)
                root.game.commander.toggle_mode();
            return true;
        case "global.menu":
            if (typeof root.mainWindowRef === 'undefined')
                return false;
            return root.mainWindowRef.request_menu_toggle();
        case "global.quicksave":
            if (!root.game.saves.quicksave)
                return false;
            root.game.saves.quicksave();
            return true;
        case "global.quickload":
            if (!root.game.saves.has_save_slot || !root.game.saves.has_save_slot("quicksave"))
                return false;
            root.game.saves.load_from_slot("quicksave");
            return true;
        case "rts.pause":
            if (typeof root.mainWindowRef === 'undefined')
                return false;
            root.mainWindowRef.game_paused = !root.mainWindowRef.game_paused;
            return true;
        case "rts.speed_up":
            if (!root.game.set_game_speed)
                return false;
            root.game.set_game_speed(GameSpeeds.stepped(root.game.time_scale, 1));
            return true;
        case "rts.speed_down":
            if (!root.game.set_game_speed)
                return false;
            root.game.set_game_speed(GameSpeeds.stepped(root.game.time_scale, -1));
            return true;
        case "rts.order_stop":
            if (!root.game.has_units_selected || !root.game.orders.stop)
                return false;
            root.game.orders.stop();
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
            if (!root.game.has_units_selected || !root.game.orders.hold)
                return false;
            root.game.orders.hold();
            return true;
        case "rts.camera_pan_up":
            begin_pan_action(actionId, event);
            root.game.camera.move(0, inputStep);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_down":
            begin_pan_action(actionId, event);
            root.game.camera.move(0, -inputStep);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_left":
            begin_pan_action(actionId, event);
            root.game.camera.move(-inputStep, 0);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_right":
            begin_pan_action(actionId, event);
            root.game.camera.move(inputStep, 0);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_rotate_left":
            root.game.camera.yaw(-yawStep);
            return true;
        case "rts.camera_rotate_right":
            root.game.camera.yaw(yawStep);
            return true;
        case "rts.camera_tilt_up":
            root.game.camera.tilt(1, shiftHeld);
            return true;
        case "rts.camera_tilt_down":
            root.game.camera.tilt(-1, shiftHeld);
            return true;
        case "rts.camera_zoom_in":
            root.game.camera.zoom(zoomStep);
            return true;
        case "rts.camera_zoom_out":
            root.game.camera.zoom(-zoomStep);
            return true;
        case "rts.camera_reset":
            if (!root.game.camera.reset)
                return false;
            root.game.camera.reset();
            return true;
        case "rts.select_all_troops":
            if (!root.game.orders.select_all_troops)
                return false;
            root.game.orders.select_all_troops();
            return true;
        case "rts.build_rotate_left":
        case "rts.build_rotate_right":
            if (!root.game.placement.construction_preview_rotatable || !root.game.placement.construction_preview_rotatable())
                return false;
            root.game.placement.on_construction_scroll(actionId === "rts.build_rotate_left" ? -1 : 1);
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
                root.game.camera.move(dx, dz);
        }
    }
}
