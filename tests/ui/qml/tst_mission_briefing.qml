import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "MissionBriefing"
    when: windowShown
    width: 800
    height: 600
    visible: true

    function test_ready_mission_opens_briefing() {
        var panel = briefingComponent.createObject(testCase, {
                "visible": false
            });
        verify(panel !== null);
        verify(panel.should_open_after_loading(true, false, true, false, false));
        panel.destroy();
    }

    function test_briefing_does_not_interrupt_other_modes() {
        var panel = briefingComponent.createObject(testCase, {
                "visible": false
            });
        verify(panel !== null);
        verify(!panel.should_open_after_loading(false, false, true, false, false), "skirmishes should not show mission briefings");
        verify(!panel.should_open_after_loading(true, true, true, false, false), "the loading screen must finish first");
        verify(!panel.should_open_after_loading(true, false, false, false, false), "do not open before the match starts");
        verify(!panel.should_open_after_loading(true, false, true, true, false), "do not cover a menu or another overlay");
        verify(!panel.should_open_after_loading(true, false, true, false, true), "do not interrupt the tutorial");
        panel.destroy();
    }

    Component {
        id: briefingComponent

        ObjectivesPanel {
        }
    }
}
