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

    // MissionsScreen fills its parent, so each case hosts it in an item of
    // the window size under test.
    Component {
        id: hostComponent

        Item {
        }
    }

    Component {
        id: missionsComponent

        MissionsScreen {
        }
    }

    function mission(title, objectives) {
        var conditions = [];
        for (var i = 0; i < objectives.length; i++)
            conditions.push({
                    "description": objectives[i]
                });
        return {
            "title": title,
            "file_path": "missions/" + title + ".json",
            "summary": "Field briefing for " + title,
            "victory_mode": "all",
            "objectives": conditions,
            "optional_objectives": [],
            "defeat_conditions": [],
            "starting_force": [],
            "starting_resources": []
        };
    }

    function create_screen(width, height, missions) {
        var host = createTemporaryObject(hostComponent, testCase, {
                "width": width,
                "height": height
            });
        var screen = missionsComponent.createObject(host, {
                "missions": missions,
                "selected_index": 0
            });
        verify(screen !== null);
        waitForRendering(screen);
        compare(screen.width, width);
        compare(screen.height, height);
        return screen;
    }

    function verify_inside_screen(screen, item, what) {
        verify(item !== null, what + " must exist");
        verify(item.visible && item.width > 0 && item.height > 0, what + " must be laid out");
        var top_left = item.mapToItem(screen, 0, 0);
        verify(top_left.x >= 0 && top_left.x + item.width <= screen.width, what + " must fit horizontally");
        verify(top_left.y >= 0 && top_left.y + item.height <= screen.height, what + " must fit vertically");
    }

    function verify_objectives_on_screen(screen, expected) {
        var objectives = findChild(screen, "selectedMissionObjectives");
        var list = findChild(screen, "selectedMissionObjectivesList");
        verify_inside_screen(screen, objectives, "the pinned objectives");
        verify(list !== null && list.visible && list.implicitHeight > 0, "the victory conditions must be rendered");
        compare(objectives.lines[0], expected);
        verify_inside_screen(screen, findChild(screen, "missionDeployButton"), "the deploy button");
    }

    function test_selected_objectives_are_visible_before_deployment() {
        var screen = create_screen(1200, 760, [mission("First", ["Hold the crossing"]), mission("Second", ["Capture the fortress"])]);
        verify_objectives_on_screen(screen, "Hold the crossing");
        screen.selected_index = 1;
        waitForRendering(screen);
        verify_objectives_on_screen(screen, "Capture the fortress");
    }

    function test_objectives_remain_visible_on_a_short_screen() {
        var screen = create_screen(900, 620, [mission("Short", ["Protect the commander"])]);
        verify_objectives_on_screen(screen, "Protect the commander");
    }

    function test_long_objective_lists_scroll_without_hiding_the_deploy_button() {
        var goals = [];
        for (var i = 0; i < 9; i++)
            goals.push("Objective number " + (i + 1) + " must be carried out before the enemy relief column arrives");
        var screen = create_screen(900, 620, [mission("Crowded", goals)]);
        verify_objectives_on_screen(screen, goals[0]);
        var scroll_bar = findChild(screen, "selectedMissionObjectivesScrollBar");
        verify(scroll_bar !== null && scroll_bar.visible, "an overflowing objective list scrolls in its own section");
    }

    function test_briefing_keeps_its_share_beside_the_field_preview() {
        var screen = create_screen(1600, 900, [mission("Wide", ["Burn the granaries"])]);
        var briefing = findChild(screen, "missionBriefing");
        var preview = findChild(screen, "missionFieldPreviewFrame");
        verify(briefing !== null && preview !== null);
        verify(preview.visible, "the reconnaissance map is shown on a wide screen");
        verify(briefing.width > preview.width, "the field preview must not swallow the briefing column (" + briefing.width + " vs " + preview.width + ")");
    }

    function test_selection_follows_the_mission_list() {
        var screen = create_screen(1200, 760, [mission("Only", ["Hold"])]);
        screen.selected_index = 5;
        screen.clamp_selection();
        compare(screen.selected_index, 0);
        screen.missions = [];
        screen.clamp_selection();
        compare(screen.selected_index, -1);
        compare(screen.selected, null);
    }
}
