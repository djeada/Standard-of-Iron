import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "MissionSelectionObjectives"
    when: windowShown
    width: 1280
    height: 800
    visible: true

    Component {
        id: missionsComponent

        MissionsScreen {
        }
    }

    function mission(title, objective) {
        return {
            "title": title,
            "file_path": "missions/" + title + ".json",
            "summary": "Field briefing for " + title,
            "victory_mode": "all",
            "objectives": [{"description": objective}],
            "optional_objectives": [],
            "defeat_conditions": [],
            "starting_force": [],
            "starting_resources": []
        };
    }

    function verify_objectives_on_screen(screen, expected) {
        var objectives = findChild(screen, "selectedMissionObjectives");
        var list = findChild(screen, "selectedMissionObjectivesList");
        verify(objectives !== null, "the selected mission needs a pinned objectives section");
        verify(list !== null, "the victory conditions need to be rendered, not just retained in data");
        verify(objectives.visible && list.visible);
        verify(objectives.height > 0 && list.implicitHeight > 0);
        var top = objectives.mapToItem(screen, 0, 0).y;
        verify(top >= 0 && top + objectives.height <= screen.height,
               "the mission objectives must be visible without scrolling the briefing");
        compare(objectives.lines[0], expected);
    }

    function test_selected_objectives_are_visible_before_deployment() {
        var screen = missionsComponent.createObject(testCase, {
                "width": 1200,
                "height": 760,
                "missions": [mission("First", "Hold the crossing"), mission("Second", "Capture the fortress")],
                "selected_index": 0
            });
        verify(screen !== null);
        waitForRendering(screen);
        verify_objectives_on_screen(screen, "Hold the crossing");
        screen.selected_index = 1;
        compare(screen.selected_index, 1);
        verify_objectives_on_screen(screen, "Capture the fortress");
        screen.destroy();
    }

    function test_objectives_remain_visible_on_a_short_screen() {
        var screen = missionsComponent.createObject(testCase, {
                "width": 900,
                "height": 620,
                "missions": [mission("Short", "Protect the commander")],
                "selected_index": 0
            });
        verify(screen !== null);
        waitForRendering(screen);
        verify_objectives_on_screen(screen, "Protect the commander");
        screen.destroy();
    }
}
