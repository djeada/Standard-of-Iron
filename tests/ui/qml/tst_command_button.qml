import QtQuick 2.15
import QtQuick.Window 2.15
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

    function requireActiveWindow() {
        var window = testCase.Window.window;
        verify(window !== null, "the test case has no window");
        tryVerify(function () {
                return window.active;
            }, 5000, "the test window never became active, so keyboard focus cannot be delivered");
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

    function test_icon_only_mode_keeps_the_full_tooltip_label_and_hotkey() {
        var button = makeButton({
                "actionId": "patrol",
                "label": "Patrol",
                "iconOnly": true,
                "hotkey": "P",
                "width": 200,
                "height": 48
            });
        verify(button.compact);
        verify(!button.showsShortLabel);
        compare(button.tooltip.title, "Patrol");
        compare(button.tooltip.hotkey, "P");
        compare(button.Accessible.name, "Patrol");
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

    function test_an_order_explains_its_rules_beyond_the_one_line_hint() {
        var button = makeButton({
                "actionId": "guard",
                "label": "Guard",
                "hint": "Anchor troops to a spot.",
                "details": [{
                        "term": "Reach",
                        "text": "They fight anything within 10 m of that spot."
                    }, {
                        "term": "Release",
                        "text": "Press Guard again."
                    }]
            });
        compare(button.details.length, 2);
        compare(button.details[0].term, "Reach");
        button.destroy();
    }

    function test_the_order_grid_is_reachable_without_a_mouse() {
        var button = makeButton({
                "actionId": "patrol",
                "label": "Patrol"
            });
        compare(button.focusPolicy, Qt.TabFocus);
        button.destroy();
    }

    function test_keyboard_focus_opens_the_same_tooltip_hovering_would() {
        var button = makeButton({
                "actionId": "patrol",
                "label": "Patrol",
                "hotkey": "P",
                "hint": "March a beat between two points.",
                "details": [{
                        "term": "Give it",
                        "text": "Click the first waypoint, then the second."
                    }],
                "statusText": "Click waypoint 1",
                "width": 200,
                "height": 48
            });
        verify(!button.tooltip.visible);
        requireActiveWindow();
        button.forceActiveFocus(Qt.TabFocusReason);
        tryVerify(function () {
                return button.tooltip.visible;
            }, 4000);
        compare(button.tooltip.title, "Patrol");
        compare(button.tooltip.hotkey, "P");
        compare(button.tooltip.summary, "March a beat between two points.");
        compare(button.tooltip.details.length, 1);
        compare(button.tooltip.status, "Click waypoint 1");
        button.focus = false;
        tryVerify(function () {
                return !button.tooltip.visible;
            }, 4000);
        button.destroy();
    }

    function test_an_order_the_selection_cannot_take_says_so_in_the_tooltip() {
        var button = makeButton({
                "actionId": "hold",
                "label": "Hold",
                "blocked": true,
                "hint": "Stand fast.",
                "disabledReason": "Hold is only available to archers and spearmen"
            });
        requireActiveWindow();
        button.forceActiveFocus(Qt.TabFocusReason);
        tryVerify(function () {
                return button.tooltip.visible;
            }, 4000);
        compare(button.tooltip.warning, "Hold is only available to archers and spearmen");
        compare(button.tooltip.summary, "Stand fast.");
        button.destroy();
    }

    function test_live_order_state_reads_on_the_button_face() {
        var button = makeButton({
                "actionId": "aura",
                "label": "Aura",
                "statusText": "Ready in 43s",
                "cooldown": 0.7
            });
        compare(button.statusText, "Ready in 43s");
        compare(button.cooldown, 0.7);
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
