import QtQuick 2.15

Item {
    id: root

    property bool active: false
    property var game
    property var gameView
    property var mainWindowRef
    property var renderAreaRef
    property var pressed_keys: ({
    })

    function begin_pan_key(e) {
        if (!e.isAutoRepeat && !pressed_keys[e.key]) {
            pressed_keys[e.key] = true;
            renderAreaRef.key_pan_count += 1;
            mainWindowRef.edge_scroll_disabled = true;
        }
    }

    function reset_pan_keys() {
        pressed_keys = ({});
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

    function handle_key_pressed(event) {
        if (!root.active)
            return ;

        if (typeof root.game === 'undefined')
            return ;

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            reset_pan_keys();
            if (root.game.toggle_commander_control_mode)
                root.game.toggle_commander_control_mode();

            event.accepted = true;
            return ;
        }

        var yawStep = (event.modifiers & Qt.ShiftModifier) ? 8 : 4;
        var inputStep = (event.modifiers & Qt.ShiftModifier) ? 2 : 1;
        var shiftHeld = (event.modifiers & Qt.ShiftModifier) !== 0;
        switch (event.key) {
        case Qt.Key_Escape:
            if (typeof root.mainWindowRef !== 'undefined' && !root.mainWindowRef.menu_visible) {
                root.mainWindowRef.menu_visible = true;
                event.accepted = true;
            }
            break;
        case Qt.Key_Space:
            if (typeof root.mainWindowRef !== 'undefined') {
                root.mainWindowRef.game_paused = !root.mainWindowRef.game_paused;
                root.gameView.set_paused(root.mainWindowRef.game_paused);
                event.accepted = true;
            }
            break;
        case Qt.Key_S:
            if (root.game.has_units_selected) {
                if (root.game.on_stop_command)
                    root.game.on_stop_command();

                event.accepted = true;
            }
            break;
        case Qt.Key_A:
            if (root.game.has_units_selected) {
                root.game.cursor_mode = "attack";
                event.accepted = true;
            }
            break;
        case Qt.Key_M:
            if (root.game.has_units_selected) {
                root.game.cursor_mode = "normal";
                event.accepted = true;
            }
            break;
        case Qt.Key_Up:
            begin_pan_key(event);
            root.game.camera_move(0, inputStep);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Down:
            begin_pan_key(event);
            root.game.camera_move(0, -inputStep);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Left:
            begin_pan_key(event);
            root.game.camera_move(-inputStep, 0);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Right:
            begin_pan_key(event);
            root.game.camera_move(inputStep, 0);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Q:
            root.game.camera_yaw(-yawStep);
            event.accepted = true;
            break;
        case Qt.Key_E:
            root.game.camera_yaw(yawStep);
            event.accepted = true;
            break;
        case Qt.Key_R:
            root.game.camera_orbit_direction(1, shiftHeld);
            event.accepted = true;
            break;
        case Qt.Key_F:
            root.game.camera_orbit_direction(-1, shiftHeld);
            event.accepted = true;
            break;
        case Qt.Key_X:
            if (root.game.select_all_troops)
                root.game.select_all_troops();

            event.accepted = true;
            break;
        case Qt.Key_P:
            if (root.game.has_units_selected) {
                root.game.cursor_mode = "patrol";
                event.accepted = true;
            }
            break;
        case Qt.Key_G:
            if (root.game.has_units_selected) {
                root.game.cursor_mode = "guard";
                event.accepted = true;
            }
            break;
        case Qt.Key_H:
            if (root.game.has_units_selected && root.game.on_hold_command) {
                root.game.on_hold_command();
                event.accepted = true;
            }
            break;
        }
    }

    function handle_key_released(event) {
        if (!root.active)
            return ;

        var movementKeys = [Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right];
        if (movementKeys.indexOf(event.key) !== -1) {
            if (pressed_keys[event.key]) {
                pressed_keys[event.key] = false;
                renderAreaRef.key_pan_count = Math.max(0, renderAreaRef.key_pan_count - 1);
            }
            var anyHeld = false;
            for (var k in pressed_keys) {
                if (pressed_keys[k]) {
                    anyHeld = true;
                    break;
                }
            }
            if (!anyHeld) {
                if (keyPanTimer.running)
                    keyPanTimer.stop();

                if (renderAreaRef.key_pan_count === 0 && !renderAreaRef.mouse_pan_active)
                    mainWindowRef.edge_scroll_disabled = false;

            }
        }
        if (event.key === Qt.Key_Shift) {
            if (renderAreaRef.key_pan_count === 0 && !renderAreaRef.mouse_pan_active)
                mainWindowRef.edge_scroll_disabled = false;

        }
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

    Keys.onPressed: function(event) {
        root.handle_key_pressed(event);
    }

    Keys.onReleased: function(event) {
        root.handle_key_released(event);
    }

    Timer {
        id: keyPanTimer

        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (typeof root.game === 'undefined')
                return ;

            var step = (Qt.inputModifiers & Qt.ShiftModifier) ? 2 : 1;
            var dx = 0;
            var dz = 0;
            if (pressed_keys[Qt.Key_Up])
                dz += step;

            if (pressed_keys[Qt.Key_Down])
                dz -= step;

            if (pressed_keys[Qt.Key_Left])
                dx -= step;

            if (pressed_keys[Qt.Key_Right])
                dx += step;

            if (dx !== 0 || dz !== 0)
                root.game.camera_move(dx, dz);

        }
    }
}
