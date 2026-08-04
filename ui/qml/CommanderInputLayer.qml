import QtQuick 2.15
import StandardOfIron 1.0

Item {
    id: root

    property var commanderInput: null
    property var gameView
    property var mainWindowRef
    property bool active: false

    property var held_keys: ({})

    signal inputCaptured

    function scene_center() {
        return root.mapToItem(null, root.width * 0.5, root.height * 0.5);
    }

    function center_mouse() {
        if (!root.active || root.commanderInput === null || !root.commanderInput.center_mouse)
            return;
        var center = scene_center();
        root.commanderInput.center_mouse(center.x, center.y);
    }

    function release_actions() {
        if (root.commanderInput === null)
            return;
        if (root.commanderInput.primary_action_up)
            root.commanderInput.primary_action_up();
        if (root.commanderInput.secondary_action_up)
            root.commanderInput.secondary_action_up();
        root.held_keys = ({});
    }

    function is_locomotion(actionId) {
        switch (actionId) {
        case "commander.move_forward":
        case "commander.move_back":
        case "commander.strafe_left":
        case "commander.strafe_right":
        case "commander.turn_left":
        case "commander.turn_right":
        case "commander.sprint":
            return true;
        }
        return false;
    }

    function perform_action(actionId, event) {
        if (actionId === "global.menu") {
            if (typeof root.mainWindowRef !== 'undefined' && !root.mainWindowRef.menu_visible)
                root.mainWindowRef.menu_visible = true;
            return true;
        }
        if (actionId === "global.toggle_control_mode") {
            if (root.commanderInput !== null && root.commanderInput.toggle_mode)
                root.commanderInput.toggle_mode();
            return true;
        }
        if (root.commanderInput === null)
            return false;
        if (is_locomotion(actionId)) {
            var canonical = InputBindings.canonical_key_for(actionId);
            if (canonical !== 0 && root.commanderInput.key_down) {
                root.held_keys[event.key] = actionId;
                root.commanderInput.key_down(canonical, event.modifiers);
                return true;
            }
            return false;
        }
        if (event.isAutoRepeat)
            return true;
        switch (actionId) {
        case "commander.dodge":
            if (root.commanderInput.dodge)
                root.commanderInput.dodge();
            return true;
        case "commander.jump":
            if (root.commanderInput.jump)
                root.commanderInput.jump();
            return true;
        case "commander.cycle_lock_on":
            if (root.commanderInput.cycle_lock_on)
                root.commanderInput.cycle_lock_on();
            return true;
        case "commander.ability_vanguard_rush":
            if (root.commanderInput.vanguard_rush)
                root.commanderInput.vanguard_rush();
            return true;
        case "commander.ability_second_wind":
            if (root.commanderInput.second_wind)
                root.commanderInput.second_wind();
            return true;
        case "commander.ability_aura":
            if (root.commanderInput.trigger_aura)
                root.commanderInput.trigger_aura();
            return true;
        case "commander.special_action":
            if (root.commanderInput.special_action)
                root.commanderInput.special_action();
            return true;
        case "commander.rally":
            if (root.commanderInput.trigger_rally)
                root.commanderInput.trigger_rally();
            return true;
        case "commander.toggle_weapon":
            if (root.commanderInput.toggle_weapon_stance)
                root.commanderInput.toggle_weapon_stance();
            return true;
        case "commander.toggle_camera_mode":
            if (root.commanderInput.toggle_camera_mode)
                root.commanderInput.toggle_camera_mode();
            return true;
        }
        return false;
    }

    function handle_key_pressed(event) {
        if (!root.active)
            return;
        var candidates = InputBindings.actions_for_key(event.key, event.modifiers, "commander");
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
        var actionId = root.held_keys[event.key];
        if (actionId === undefined)
            return;
        delete root.held_keys[event.key];
        var canonical = InputBindings.canonical_key_for(actionId);
        if (canonical !== 0 && root.commanderInput !== null && root.commanderInput.key_up)
            root.commanderInput.key_up(canonical, event.modifiers);
        event.accepted = true;
    }

    function handle_mouse(button, modifiers, pressed) {
        if (root.commanderInput === null)
            return;
        var candidates = InputBindings.actions_for_mouse(button, modifiers, "commander");
        for (var i = 0; i < candidates.length; ++i) {
            if (candidates[i] === "commander.primary_action") {
                if (pressed && root.commanderInput.primary_action_down)
                    root.commanderInput.primary_action_down();
                else if (!pressed && root.commanderInput.primary_action_up)
                    root.commanderInput.primary_action_up();
                return;
            }
            if (candidates[i] === "commander.secondary_action") {
                if (pressed && root.commanderInput.secondary_action_down)
                    root.commanderInput.secondary_action_down();
                else if (!pressed && root.commanderInput.secondary_action_up)
                    root.commanderInput.secondary_action_up();
                return;
            }
        }
    }

    enabled: active
    visible: active
    z: 20
    Component.onCompleted: {
        if (active && typeof root.gameView !== 'undefined')
            root.gameView.forceActiveFocus();
    }
    onActiveChanged: {
        if (active) {
            if (typeof root.gameView !== 'undefined')
                root.gameView.forceActiveFocus();
            root.inputCaptured();
            center_mouse();
        } else {
            release_actions();
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        cursorShape: Qt.BlankCursor
        enabled: root.active
        hoverEnabled: true
        preventStealing: true
        onEntered: {
            root.inputCaptured();
            root.center_mouse();
        }
        onPositionChanged: function (mouse) {
            root.inputCaptured();
            mouse.accepted = true;
        }
        onWheel: function (w) {
            w.accepted = true;
        }
        onPressed: function (mouse) {
            if (typeof root.gameView !== 'undefined')
                root.gameView.forceActiveFocus();
            root.inputCaptured();
            root.handle_mouse(mouse.button, mouse.modifiers, true);
            mouse.accepted = true;
        }
        onReleased: function (mouse) {
            root.handle_mouse(mouse.button, mouse.modifiers, false);
            mouse.accepted = true;
        }
        onCanceled: root.release_actions()
    }

    focus: false
    Keys.enabled: active
    Keys.onPressed: function (event) {
        root.handle_key_pressed(event);
    }

    Keys.onReleased: function (event) {
        root.handle_key_released(event);
    }
}
