import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "CommandTooltip"
    when: windowShown
    width: 400
    height: 200
    visible: true

    function init() {
        Core.UiPreferences.reset_to_defaults();
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function makeTooltip(props) {
        return tooltipComponent.createObject(testCase, props || {});
    }

    function test_a_bare_title_is_not_worth_showing() {
        var tip = makeTooltip({
                "title": "Guard"
            });
        verify(!tip.hasBody);
        tip.destroy();
    }

    function test_a_summary_alone_is_worth_showing() {
        var tip = makeTooltip({
                "title": "Hold",
                "summary": "Troops dig in where they stand."
            });
        verify(tip.hasBody);
        tip.destroy();
    }

    function test_rules_alone_are_worth_showing() {
        var tip = makeTooltip({
                "title": "Patrol",
                "details": [{
                        "term": "Give it",
                        "text": "Click the first waypoint, then the second."
                    }]
            });
        verify(tip.hasBody);
        compare(tip.detail_term(tip.details[0]), "Give it");
        compare(tip.detail_text(tip.details[0]), "Click the first waypoint, then the second.");
        tip.destroy();
    }

    function test_a_live_status_is_worth_showing_on_its_own() {
        var tip = makeTooltip({
                "title": "Aura",
                "status": "Ready in 43s"
            });
        verify(tip.hasBody);
        tip.destroy();
    }

    function test_a_refusal_is_worth_showing_on_its_own() {
        var tip = makeTooltip({
                "title": "Hold",
                "warning": "Hold is only available to archers and spearmen"
            });
        verify(tip.hasBody);
        tip.destroy();
    }

    function test_the_rules_grow_with_the_interface_scale() {
        var props = {
            "title": "Guard",
            "summary": "Anchor troops to a spot they will hold.",
            "details": [{
                    "term": "Reach",
                    "text": "They fight what comes within 10 m of that spot, and drop a target that leaves it."
                }]
        };
        var small = makeTooltip(props);
        var narrow = small.implicitWidth;
        var flat = small.implicitHeight;
        small.destroy();
        Core.UiPreferences.uiScale = 1.75;
        var large = makeTooltip(props);
        verify(large.implicitWidth > narrow);
        verify(large.implicitHeight > flat);
        large.destroy();
    }

    function test_a_missing_rule_does_not_break_the_layout() {
        var tip = makeTooltip({
                "title": "Guard",
                "details": [{}, null]
            });
        compare(tip.detail_term(tip.details[0]), "");
        compare(tip.detail_text(tip.details[1]), "");
        tip.destroy();
    }

    Component {
        id: tooltipComponent

        IronCommandTooltip {
        }
    }
}
