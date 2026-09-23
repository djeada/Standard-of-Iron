import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "TutorialOverlay"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    QtObject {
        id: fakeTutorial

        property bool active: true
        property bool visible: true
        property bool finished: false
        property int step_index: 7
        property int step_count: 15
        property string title: "Assemble an army"
        property string body: "One soldier is not an army. Keep recruiting until you field eight - mix spearmen to hold a line with archers to punish whatever charges it. If a card turns grey, the tutorial hint tells you what ran short. New recruits gather at the barracks' rally flag; set one from the production panel."
        property string objective: "Field at least 8 soldiers"
        property string objective_state: "active"
        property string progress_text: "7 / 8"
        property real progress: 0.875
        property string hint: ""
        property bool step_complete: false
        property bool has_focus_point: false
        property var focus_points: []

        function set_visible(v) {
            visible = v;
        }
        function skip_step() {
        }
        function replay_step() {
        }
        function continue_step() {
        }
        function stop() {
        }
    }

    TutorialOverlay {
        id: overlay

        tutorial: fakeTutorial
        max_height: 360
    }

    function inside_card(button) {
        var p = button.mapToItem(overlay, 0, 0);
        return button.visible && p.y >= 0 && p.y + button.height <= overlay.height + 0.5;
    }

    function init() {
        fakeTutorial.hint = "";
        fakeTutorial.has_focus_point = false;
    }

    function test_skip_and_end_stay_on_the_card_when_a_long_hint_appears() {
        fakeTutorial.hint = "The barracks has only 0 reserve left to draw on. Every recruit costs reserve; when it runs dry, build Homes - each Home raises families, and a civilian recruited there and sent to the barracks with Deliver refills it.";
        wait(50);
        var skip = findChild(overlay, "tutorialSkipButton");
        var end = findChild(overlay, "tutorialEndButton");
        verify(skip !== null && end !== null);
        verify(overlay.height <= overlay.max_height + 0.5, "the card outgrew its room: " + overlay.height);
        verify(inside_card(skip), "Skip step was pushed off the card");
        verify(inside_card(end), "End tutorial was pushed off the card");
    }

    function test_the_skip_button_does_not_move_when_the_hint_changes() {
        wait(50);
        var skip = findChild(overlay, "tutorialSkipButton");
        var before = skip.mapToItem(overlay, 0, 0).x;
        fakeTutorial.has_focus_point = true;
        wait(50);
        compare(skip.mapToItem(overlay, 0, 0).x, before, "Show me appearing slid Skip sideways under the cursor");
    }
}
