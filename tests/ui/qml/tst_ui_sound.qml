import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "UiSound"
    when: windowShown
    width: 400
    height: 300
    visible: true

    property var played: []

    QtObject {
        id: recorder

        function play_cue(cue_id) {
            testCase.played.push(cue_id);
        }
    }

    function init() {
        testCase.played = [];
        UiSound.audioSystem = recorder;
        UiSound.enabled = true;
    }

    function cleanup() {
        UiSound.audioSystem = null;
    }

    function make(component, props) {
        return component.createObject(testCase, props || {});
    }

    Component {
        id: plainButton

        IronButton {
            text: "Order"
        }
    }

    Component {
        id: overridingButton

        IronButton {
            property int callerClicks: 0

            text: "Order"
            onClicked: callerClicks++
        }
    }

    Component {
        id: overridingCheckBox

        IronCheckBox {
            property int callerToggles: 0

            text: "Reduce motion"
            onToggled: callerToggles++
        }
    }

    Component {
        id: listRow

        IronListRow {
            text: "Slot 1"
        }
    }

    Component {
        id: tabBar

        IronTabBar {
            tabs: ["Units", "Buildings"]
        }
    }

    Component {
        id: dialog

        IronDialog {
            title: "Error"
        }
    }

    Component {
        id: dangerDialog

        IronDialog {
            title: "Error"
            tone: "danger"
        }
    }

    Component {
        id: warningDialog

        IronDialog {
            title: "Careful"
            tone: "warning"
        }
    }

    function test_a_button_click_is_audible() {
        var button = make(plainButton);
        button.clicked();
        compare(testCase.played, ["ui.click"]);
        button.destroy();
    }

    function test_a_caller_handler_does_not_mute_the_button() {
        var button = make(overridingButton);
        button.clicked();
        compare(button.callerClicks, 1, "the caller's own handler still runs");
        compare(testCase.played, ["ui.click"], "and the sound still plays");
        button.destroy();
    }

    function test_a_caller_handler_does_not_mute_the_checkbox() {
        var box = make(overridingCheckBox);
        box.toggled();
        compare(box.callerToggles, 1);
        compare(testCase.played, ["ui.toggle"]);
        box.destroy();
    }

    function test_a_list_row_answers_the_pointer() {
        var row = make(listRow);
        row.clicked();
        compare(testCase.played, ["ui.click"]);
        row.destroy();
    }

    function test_switching_tabs_is_audible() {
        var bar = make(tabBar, {
                "currentIndex": 0
            });
        testCase.played = [];
        bar.currentIndex = 1;
        compare(testCase.played, ["ui.tab_switch"]);
        bar.destroy();
    }

    function test_a_dialog_announces_open_and_close() {
        var box = make(dialog);
        box.open();
        box.close();
        compare(testCase.played, ["ui.panel_open", "ui.panel_close"]);
        box.destroy();
    }

    Component {
        id: blockedButton

        IronButton {
            property int callerClicks: 0

            text: "Start"
            blocked: true
            disabledReason: "Finish the previous mission first."
            onClicked: callerClicks++
        }
    }

    function test_a_blocked_control_refuses_out_loud() {
        var button = make(blockedButton, {
                "width": 120,
                "height": 40
            });
        verify(button.enabled, "a blocked control stays enabled or it hears nothing");
        verify(!button.interactive);
        mousePress(button, 10, 10);
        mouseRelease(button, 10, 10);
        compare(testCase.played, ["ui.error"], "the click is answered by a refusal");
        compare(button.callerClicks, 0, "and the blocked action never runs");
        button.destroy();
    }

    function test_unblocking_a_control_restores_the_action() {
        var button = make(blockedButton, {
                "width": 120,
                "height": 40
            });
        button.blocked = false;
        verify(button.interactive);
        mousePress(button, 10, 10);
        mouseRelease(button, 10, 10);
        compare(testCase.played, ["ui.click"]);
        compare(button.callerClicks, 1);
        button.destroy();
    }

    function test_a_refusing_dialog_sounds_like_a_refusal() {
        var box = make(dangerDialog);
        box.open();
        compare(testCase.played, ["ui.error"], "a danger dialog is the game saying no");
        box.destroy();
        testCase.played = [];
        var caution = make(warningDialog);
        caution.open();
        compare(testCase.played, ["ui.error"]);
        caution.destroy();
    }

    function test_muting_the_bus_silences_every_control() {
        UiSound.enabled = false;
        var button = make(plainButton);
        button.clicked();
        compare(testCase.played, []);
        button.destroy();
    }
}
