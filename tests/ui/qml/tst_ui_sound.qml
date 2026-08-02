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

    function test_muting_the_bus_silences_every_control() {
        UiSound.enabled = false;
        var button = make(plainButton);
        button.clicked();
        compare(testCase.played, []);
        button.destroy();
    }
}
