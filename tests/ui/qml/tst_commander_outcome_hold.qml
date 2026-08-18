import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0 as Design

TestCase {
    id: testCase

    name: "CommanderOutcomeHold"
    when: windowShown
    width: 600
    height: 400
    visible: true

    Design.IronOutcomeOverlay {
        id: overlay

        anchors.fill: parent
        victoryState: ""
        held: false
    }

    function test_banner_waits_for_the_closing_line() {
        overlay.victoryState = "";
        overlay.held = false;
        verify(!overlay.visible, "no outcome yet");
        overlay.held = true;
        overlay.victoryState = "victory";
        verify(!overlay.visible, "banner must wait while a commander is speaking");
        overlay.held = false;
        verify(overlay.visible, "banner appears once the line is done");
    }

    function test_report_is_unreachable_while_held() {
        overlay.victoryState = "defeat";
        overlay.held = true;
        verify(!overlay.visible);
        verify(!overlay.showingSummary, "the battle report cannot be opened behind a held banner");
    }
}
