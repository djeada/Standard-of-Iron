import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "FormationPanelInput"
    when: windowShown
    width: 1000
    height: 700
    visible: true

    readonly property var placement_options: ({
            "intent": "faction_default",
            "doctrine_display_name": "Legion",
            "unit_count": 4,
            "placed_count": 4,
            "slot_count": 4,
            "blocked_slots": 0,
            "adjusted_slots": 0,
            "ranks": 2,
            "files": 2,
            "plan_frontage": 6,
            "plan_depth": 3,
            "plan_valid": true,
            "single_unit": false,
            "facing_degrees": 90,
            "gesture": "right_drag"
        })

    Component {
        id: sceneComponent

        Item {
            id: scene

            property int world_moves: 0
            property int world_presses: 0
            property alias panel: panel
            property alias world_area: world

            width: 1000
            height: 700

            MouseArea {
                id: world

                objectName: "world"
                acceptedButtons: Qt.AllButtons
                anchors.fill: parent
                hoverEnabled: true
                onPositionChanged: scene.world_moves++
                onPressed: scene.world_presses++
            }

            FormationPanel {
                id: panel

                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                anchors.left: parent.left
                anchors.leftMargin: 16
                intents: ["faction_default", "line", "column", "defensive"]
                options: testCase.placement_options
                placing: true
            }
        }
    }

    function make_scene() {
        var scene = createTemporaryObject(sceneComponent, testCase);
        verify(scene !== null, "the scene failed to instantiate");
        waitForRendering(scene);
        verify(scene.panel.visible, "the formation panel must be up while placing");
        return scene;
    }

    function panel_point(scene, dx, dy) {
        return scene.panel.mapToItem(scene, dx, dy);
    }

    function test_the_ground_under_the_panel_is_not_aimed_at_through_it() {
        var scene = make_scene();
        mouseMove(scene, 700, 120);
        mouseMove(scene, 702, 122);
        verify(scene.world_area.containsMouse, "the world must still follow the pointer over open ground");
        var body = panel_point(scene, scene.panel.width / 2, 6);
        mouseMove(scene, body.x, body.y);
        mouseMove(scene, body.x + 2, body.y + 2);
        verify(!scene.world_area.containsMouse, "the pointer over the panel must not drag the formation anchor across the map");
        mouseMove(scene, 700, 120);
        verify(scene.world_area.containsMouse, "leaving the panel hands the pointer back to the battlefield");
    }

    function test_a_click_on_the_panel_is_not_a_click_on_the_battlefield() {
        var scene = make_scene();
        var body = panel_point(scene, scene.panel.width / 2, 6);
        mousePress(scene, body.x, body.y);
        mouseRelease(scene, body.x, body.y);
        compare(scene.world_presses, 0, "a click on the panel must not deploy the formation under the panel");
        mousePress(scene, 700, 120);
        mouseRelease(scene, 700, 120);
        compare(scene.world_presses, 1, "a click on open ground still reaches the battlefield");
    }

    function test_the_panel_still_reports_the_formation_the_pointer_is_on() {
        var scene = make_scene();
        var chips = [];
        find_chips(scene.panel, chips);
        verify(chips.length >= 4, "the panel must offer one card per formation");
        var chip = chips[1];
        var centre = chip.mapToItem(scene, chip.width / 2, chip.height / 2);
        mouseMove(scene, centre.x, centre.y);
        compare(scene.panel.hovered_intent, "line", "hovering a card must describe that formation");
        compare(scene.world_presses, 0);
    }

    function find_chips(item, out) {
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];
            if (child.toString().indexOf("QQuickRectangle") === 0 && child.blocked_reason !== undefined)
                out.push(child);
            find_chips(child, out);
        }
    }
}
