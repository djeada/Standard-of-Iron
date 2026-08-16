import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "FormationPanel"
    when: windowShown
    width: 900
    height: 700
    visible: true

    readonly property var all_intents: ["faction_default", "line", "column", "defensive", "assault", "encirclement", "siege_escort"]

    function makePanel(props) {
        return panelComponent.createObject(testCase, props);
    }

    function test_showing_a_populated_planner_survives_layout() {
        var panel = makePanel({
                "placing": true,
                "intents": testCase.all_intents,
                "options": {
                    "intent": "line",
                    "unit_count": 12,
                    "slot_count": 12,
                    "placed_count": 12,
                    "ranks": 3,
                    "files": 4,
                    "plan_frontage": 18,
                    "plan_depth": 6,
                    "plan_valid": true
                }
            });
        wait(50);
        verify(panel.visible);
        verify(panel.width > 0);
        verify(panel.height > 0);
        panel.advanced_expanded = true;
        wait(50);
        verify(panel.height > 0);
        panel.destroy();
    }

    function test_the_panel_stays_inside_the_band_the_hud_gives_it() {
        var panel = makePanel({
                "placing": true,
                "intents": testCase.all_intents,
                "max_height": 260,
                "options": {
                    "intent": "line",
                    "slot_count": 12
                }
            });
        wait(50);
        verify(panel.height <= 260, "collapsed height " + panel.height);
        var collapsed = panel.height;
        panel.advanced_expanded = true;
        wait(50);
        verify(panel.height <= 260, "expanded height " + panel.height);
        verify(panel.height >= collapsed - 1);
        panel.max_height = 0;
        wait(50);
        verify(panel.height > 260);
        panel.destroy();
    }

    function test_every_intent_has_its_own_silhouette() {
        var panel = makePanel({});
        var seen = ({});
        for (var i = 0; i < testCase.all_intents.length; ++i) {
            var shape = panel.intent_shape(testCase.all_intents[i]);
            compare(shape.length, 28, testCase.all_intents[i]);
            verify(shape.indexOf("#") >= 0 || shape.indexOf("@") >= 0);
            verify(seen[shape] === undefined, "duplicate silhouette for " + testCase.all_intents[i]);
            seen[shape] = true;
        }
        panel.destroy();
    }

    function test_every_intent_explains_itself() {
        var panel = makePanel({});
        for (var i = 0; i < testCase.all_intents.length; ++i)
            verify(panel.intent_purpose(testCase.all_intents[i]).length > 0, testCase.all_intents[i]);
        panel.destroy();
    }

    function test_hovering_a_card_describes_it_without_selecting_it() {
        var panel = makePanel({
                "placing": true,
                "intents": testCase.all_intents,
                "options": {
                    "intent": "line"
                }
            });
        compare(panel.described_intent, "line");
        panel.hovered_intent = "column";
        compare(panel.described_intent, "column");
        compare(panel.active_intent, "line");
        panel.hovered_intent = "";
        compare(panel.described_intent, "line");
        panel.destroy();
    }

    function test_doctrine_choice_defaults_to_automatic_until_locked() {
        var panel = makePanel({
                "doctrines": [{
                        "id": "",
                        "name": "Automatic"
                    }, {
                        "id": "carthage",
                        "name": "Carthage"
                    }, {
                        "id": "rome",
                        "name": "Rome"
                    }],
                "options": {
                    "doctrine": "rome",
                    "doctrine_locked": false
                }
            });
        compare(panel.doctrine_index, 0);
        compare(panel.doctrine_names.length, 3);
        panel.options = {
            "doctrine": "rome",
            "doctrine_locked": true
        };
        compare(panel.doctrine_index, 2);
        compare(panel.doctrine_id_at(2), "rome");
        panel.options = {
            "doctrine": "not_a_doctrine",
            "doctrine_locked": true
        };
        compare(panel.doctrine_index, 0);
        panel.destroy();
    }

    function test_one_unit_is_positioned_not_offered_an_army_formation() {
        var panel = makePanel({
                "placing": true,
                "intents": testCase.all_intents,
                "options": {
                    "intent": "faction_default",
                    "unit_count": 1,
                    "single_unit": true,
                    "unit_label": "Hastati",
                    "gesture": "right_drag",
                    "facing_degrees": 90,
                    "facing_explicit": true,
                    "slot_count": 1,
                    "placed_count": 1,
                    "plan_valid": true
                }
            });
        wait(50);
        verify(panel.single_unit);
        verify(panel.height > 0);
        var hint = panel.gesture_hint();
        verify(hint.indexOf("Release") >= 0, hint);
        verify(hint.indexOf("Click to deploy") < 0, hint);
        verify(hint.indexOf("depth") < 0, hint);
        compare(panel.heading_text(), "90°");
        panel.advanced_expanded = true;
        wait(50);
        var single_height = panel.height;
        panel.options = {
            "intent": "line",
            "unit_count": 12,
            "single_unit": false,
            "gesture": "click",
            "slot_count": 12,
            "placed_count": 12,
            "plan_valid": true
        };
        wait(50);
        verify(!panel.single_unit);
        verify(panel.height > single_height, "army planner " + panel.height + " vs single " + single_height);
        verify(panel.gesture_hint().indexOf("Click to deploy") >= 0);
        panel.destroy();
    }

    function test_the_hint_describes_the_gesture_that_opened_the_planner() {
        var panel = makePanel({
                "options": {
                    "gesture": "right_drag",
                    "unit_count": 4
                }
            });
        verify(panel.right_drag_gesture);
        verify(panel.gesture_hint().indexOf("Release") >= 0);
        panel.options = {
            "gesture": "click",
            "unit_count": 4
        };
        verify(!panel.right_drag_gesture);
        verify(panel.gesture_hint().indexOf("Click to deploy") >= 0);
        panel.destroy();
    }

    function test_heading_wraps_into_a_compass_range() {
        var panel = makePanel({
                "options": {
                    "facing_degrees": -90
                }
            });
        compare(panel.heading_text(), "270°");
        panel.options = {
            "facing_degrees": 370
        };
        compare(panel.heading_text(), "10°");
        panel.destroy();
    }

    function test_shape_draws_one_dot_per_marked_cell() {
        var shape = shapeComponent.createObject(testCase, {
                "pattern": "#......" + "......." + "......." + "......@"
            });
        wait(50);
        var lit = 0;
        var cells = 0;
        for (var i = 0; i < shape.children.length; ++i) {
            var dot = shape.children[i];
            if (dot.cell === undefined)
                continue;
            ++cells;
            if (dot.visible)
                ++lit;
        }
        compare(cells, 28);
        compare(lit, 2);
        shape.destroy();
    }

    Component {
        id: panelComponent

        FormationPanel {
        }
    }

    Component {
        id: shapeComponent

        FormationShape {
        }
    }
}
