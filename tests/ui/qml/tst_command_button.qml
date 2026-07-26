import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "CommandButton"
    when: windowShown
    width: 400
    height: 200
    visible: true

    function makeButton(props) {
        return buttonComponent.createObject(testCase, props || {});
    }

    function test_art_and_glyph_follow_the_action_id() {
        var button = makeButton({
                "actionId": "attack",
                "label": "Attack"
            });
        compare(button.iconSource.toString(), Icons.command("attack").toString());
        compare(button.glyph, Icons.commandGlyph("attack"));
        button.destroy();
    }

    function test_an_unavailable_order_explains_itself() {
        var button = makeButton({
                "actionId": "build",
                "label": "Build",
                "enabled": false,
                "disabledReason": "Build is only available to builders"
            });
        compare(button.Accessible.description, "Build is only available to builders");
        compare(button.tooltipText, "Build is only available to builders");
        button.destroy();
    }

    function test_an_available_order_advertises_what_it_does() {
        var button = makeButton({
                "actionId": "patrol",
                "label": "Patrol",
                "hint": "Patrol between waypoints."
            });
        compare(button.tooltipText, "Patrol between waypoints.");
        button.destroy();
    }

    function test_a_partial_selection_states_the_split() {
        var button = makeButton({
                "actionId": "attack",
                "label": "Attack",
                "mixed": true,
                "eligibleCount": 8,
                "activeCount": 3
            });
        compare(button.coverageText, "3 of 8");
        compare(button.Accessible.description, "3 of 8");
        button.destroy();
    }

    function test_a_full_selection_does_not_print_a_split() {
        var button = makeButton({
                "actionId": "attack",
                "label": "Attack",
                "eligibleCount": 8,
                "activeCount": 8
            });
        compare(button.coverageText, "");
        button.destroy();
    }

    function test_an_untouched_selection_does_not_print_a_split() {
        var button = makeButton({
                "actionId": "attack",
                "label": "Attack",
                "eligibleCount": 8,
                "activeCount": 0
            });
        compare(button.coverageText, "");
        button.destroy();
    }

    function test_active_armed_and_idle_states_are_visually_distinct() {
        var idle = makeButton({
                "actionId": "guard",
                "label": "Guard"
            });
        var active = makeButton({
                "actionId": "guard",
                "label": "Guard",
                "active": true
            });
        var placing = makeButton({
                "actionId": "guard",
                "label": "Guard",
                "placing": true
            });
        verify(!idle.highlighted);
        verify(active.highlighted);
        verify(placing.highlighted);
        verify(idle.stateColor.toString() !== active.stateColor.toString());
        verify(active.stateColor.toString() !== placing.stateColor.toString());
        idle.destroy();
        active.destroy();
        placing.destroy();
    }

    function test_a_disabled_order_reads_as_disabled_not_merely_dim() {
        var button = makeButton({
                "actionId": "rally",
                "label": "Rally",
                "enabled": false
            });
        compare(button.stateColor.toString(), Theme.textDisabled.toString());
        button.destroy();
    }

    function test_the_active_state_is_exposed_to_assistive_tech() {
        var button = makeButton({
                "actionId": "hold",
                "label": "Hold",
                "active": true
            });
        compare(button.Accessible.name, "Hold");
        verify(button.Accessible.checked);
        button.destroy();
    }

    Component {
        id: buttonComponent

        IronCommandButton {
        }
    }
}
