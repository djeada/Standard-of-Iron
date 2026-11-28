import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "CommanderInputCapture"

    QtObject {
        id: windowState
        property bool active: true
        property bool simulation_suspended: false
    }

    QtObject {
        id: commanderStub
        property int down: 0
        property int up: 0
        property int releasedAll: 0

        function primary_action_down() {
            down++;
        }

        function primary_action_up() {
            up++;
        }

        function key_down(key, modifiers) {
            down++;
        }

        function key_up(key, modifiers) {
            up++;
        }

        function release_input() {
            releasedAll++;
        }
    }

    CommanderInputLayer {
        id: input
        commander: commanderStub
        mainWindowRef: windowState
        active: true
    }

    function init() {
        windowState.simulation_suspended = false;
        commanderStub.down = 0;
        commanderStub.up = 0;
        commanderStub.releasedAll = 0;
    }

    function test_modifier_change_does_not_swallow_release() {
        input.handle_mouse(Qt.LeftButton, Qt.NoModifier, true);
        input.handle_mouse(Qt.LeftButton, Qt.ShiftModifier, false);
        compare(commanderStub.down, 1);
        compare(commanderStub.up, 1);
    }

    function test_auto_repeat_does_not_restart_or_end_movement() {
        var press = {
            "key": Qt.Key_W,
            "modifiers": Qt.NoModifier,
            "isAutoRepeat": false
        };
        var repeat = {
            "key": Qt.Key_W,
            "modifiers": Qt.NoModifier,
            "isAutoRepeat": true
        };
        verify(input.perform_action("commander.move_forward", press));
        verify(input.handle_key_released(repeat));
        verify(input.perform_action("commander.move_forward", repeat));
        compare(commanderStub.down, 1);
        compare(commanderStub.up, 0);
        verify(input.handle_key_released(press));
        compare(commanderStub.up, 1);
    }

    function test_suspend_cancels_draw_without_shooting() {
        input.handle_mouse(Qt.LeftButton, Qt.NoModifier, true);
        compare(commanderStub.down, 1);
        windowState.simulation_suspended = true;
        compare(input.pointer_owned, false);
        compare(commanderStub.releasedAll, 1);
        windowState.simulation_suspended = false;
        compare(input.pointer_owned, true);
        input.handle_mouse(Qt.LeftButton, Qt.NoModifier, false);
        compare(commanderStub.up, 0);
    }
}
