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

    function test_selected_objectives_are_visible_before_deployment() {
        var screen = missionsComponent.createObject(testCase, {
                "width": 1200,
                "height": 760,
                "missions": [mission("First", "Hold the crossing"), mission("Second", "Capture the fortress")],
                "selected_index": 0
            });
        verify(screen !== null);
        waitForRendering(screen);
        var objectives = findChild(screen, "selectedMissionObjectives");
        verify(objectives !== null, "the mission selection screen needs a pinned objectives panel");
        verify(objectives.visible);
        verify(objectives.height > 0);
        compare(objectives.lines.length, 1);
        compare(objectives.lines[0], "Hold the crossing");
        screen.selected_index = 1;
        tryCompare(screen, "selected_index", 1);
        compare(objectives.lines[0], "Capture the fortress", "objectives must follow the highlighted mission before starting it");
        verify(objectives.visible);
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
        var objectives = findChild(screen, "selectedMissionObjectives");
        verify(objectives !== null);
        verify(objectives.visible);
        verify(objectives.height >= 68, "small viewports must still reserve room for the mission goal");
        compare(objectives.lines[0], "Protect the commander");
        screen.destroy();
    }
}
