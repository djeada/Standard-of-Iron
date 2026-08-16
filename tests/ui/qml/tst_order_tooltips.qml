import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "OrderTooltips"
    when: windowShown
    width: 1200
    height: 400
    visible: true

    property var panel: null

    function init() {
        panel = hudComponent.createObject(testCase, {});
        verify(panel !== null);
    }

    function cleanup() {
        if (panel) {
            panel.destroy();
            panel = null;
        }
    }

    function entry_for(id) {
        var commands = panel.commands;
        for (var i = 0; i < commands.length; ++i) {
            if (commands[i].id === id)
                return commands[i];
        }
        return null;
    }

    function state_with(detail, extra) {
        var state = {
            "enabled": true,
            "active": false,
            "mixed": false,
            "placing": false,
            "passive": false,
            "eligibleCount": 1,
            "activeCount": 0,
            "readyCount": 1,
            "detail": detail
        };
        for (var key in extra || {})
            state[key] = extra[key];
        return state;
    }

    function text_of(details, term) {
        for (var i = 0; i < details.length; ++i) {
            if (details[i].term === term)
                return details[i].text;
        }
        return "";
    }

    function test_guard_quotes_the_ring_the_simulation_uses() {
        var details = panel.command_details(entry_for("guard"), state_with({
                    "radius": 7
                }));
        verify(text_of(details, "Reach").indexOf("7 m") >= 0);
        verify(text_of(details, "Release") !== "");
        verify(text_of(details, "Troops") !== "");
        verify(text_of(details, "vs Hold").indexOf("Hold") >= 0);
    }

    function test_hold_states_what_digging_in_is_worth() {
        var details = panel.command_details(entry_for("hold"), state_with({
                    "archerRangeBonusPercent": 50,
                    "spearmanRangeBonusPercent": 100,
                    "damageBonusPercent": 50,
                    "healthBonusPercent": 20
                }));
        var dugIn = text_of(details, "Dug in");
        verify(dugIn.indexOf("50%") >= 0);
        verify(dugIn.indexOf("100%") >= 0);
        verify(dugIn.indexOf("20%") >= 0);
        verify(text_of(details, "Troops").indexOf("spearmen") >= 0);
        verify(text_of(details, "vs Guard").indexOf("Guard") >= 0);
    }

    function test_patrol_walks_the_player_through_the_two_clicks() {
        var entry = entry_for("patrol");
        verify(text_of(panel.command_details(entry, state_with({})), "Give it").indexOf("first waypoint") >= 0);
        compare(panel.command_status(entry, state_with({
                        "waypointStage": 1
                    })), "Click waypoint 1");
        compare(panel.command_status(entry, state_with({
                        "waypointStage": 2
                    })), "Click waypoint 2");
        compare(panel.command_status(entry, state_with({}, {
                        "active": true
                    })), "On patrol");
    }

    function test_the_aura_prints_its_own_numbers_and_its_timer() {
        var entry = entry_for("aura");
        var detail = {
            "radius": 13,
            "duration": 12,
            "remaining": 0,
            "cooldown": 40,
            "cooldownRemaining": 10,
            "summary": "Nearby spearmen regenerate health."
        };
        var details = panel.command_details(entry, state_with(detail));
        compare(text_of(details, "Effect"), "Nearby spearmen regenerate health.");
        verify(text_of(details, "Reach").indexOf("13 m") >= 0);
        compare(text_of(details, "Lasts"), "12s");
        verify(text_of(details, "Recharge").indexOf("40s") >= 0);
        compare(panel.command_status(entry, state_with(detail)), "Ready in 10s");
        compare(panel.command_cooldown(entry, state_with(detail)), 0.25);
        detail.remaining = 5;
        detail.cooldownRemaining = 0;
        compare(panel.command_status(entry, state_with(detail, {
                        "active": true
                    })), "Active 5s");
        compare(panel.command_cooldown(entry, state_with(detail)), 0);
    }

    function test_an_order_without_live_facts_still_explains_itself() {
        var details = panel.command_details(entry_for("stop"), state_with({}));
        verify(details.length > 0);
        verify(text_of(details, "Keeps").indexOf("Guard") >= 0);
    }

    Component {
        id: hudComponent

        HUDBottom {
            width: 1200
            height: 300
        }
    }
}
