import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "ContextualCommandStability"
    when: windowShown
    width: 1200
    height: 720
    visible: true

    Component {
        id: commandDeck

        HUDBottom {
            width: 1100
            height: 260
            has_movable_units: true
        }
    }

    Component {
        id: signalSpy

        SignalSpy {
        }
    }

    function state(eligible, active) {
        return {
            "enabled": true,
            "eligibleCount": eligible,
            "activeCount": active,
            "active": active > 0,
            "mixed": false,
            "placing": false,
            "passive": false,
            "detail": ({})
        };
    }

    function builder_states(collecting) {
        return {
            "build": state(1, 0),
            "collect": state(1, collecting),
            "repair": state(1, 0),
            "deliver": state(1, 0)
        };
    }

    // One HUD poll. Every 100 ms HUD.qml bumps selection_tick and
    // update_action_states() replaces action_states with a fresh map from the
    // game; there is no game here, so hand over the fresh map directly.
    function poll(deck, collecting) {
        deck.action_states = builder_states(collecting);
    }

    function create_deck() {
        var deck = createTemporaryObject(commandDeck, testCase);
        verify(deck !== null);
        deck.action_states = builder_states(0);
        waitForRendering(deck);
        return deck;
    }

    function test_polling_keeps_the_same_model_and_buttons() {
        var deck = create_deck();
        var model = deck.contextualCommands;
        var button = findChild(deck, "contextCommand_collect");
        verify(button !== null, "the contextual command was not created");
        for (var i = 0; i < 3; ++i)
            poll(deck, i % 2);
        verify(deck.contextualCommands === model, "an unchanged command set must not rebuild the Repeater model");
        verify(findChild(deck, "contextCommand_collect") === button, "polling must not recreate the buttons");
        poll(deck, 1);
        compare(button.activeCount, 1, "buttons still follow live action state");
    }

    function test_a_click_held_across_polls_still_lands() {
        var deck = create_deck();
        var button = findChild(deck, "contextCommand_build");
        verify(button !== null && button.interactive);
        var clicks = signalSpy.createObject(testCase, {
                "target": button,
                "signalName": "clicked"
            });
        mousePress(button, button.width / 2, button.height / 2);
        for (var i = 0; i < 3; ++i) {
            wait(110);
            poll(deck, 0);
        }
        mouseRelease(findChild(deck, "contextCommand_build"), button.width / 2, button.height / 2);
        compare(clicks.count, 1, "a poll between press and release swallowed the click");
        clicks.destroy();
    }

    function test_membership_changes_still_refresh_the_deck() {
        var deck = create_deck();
        var initial = deck.contextualCommands;
        compare(initial.length, 4);
        var states = builder_states(0);
        delete states.repair;
        deck.action_states = states;
        verify(deck.contextualCommands !== initial);
        compare(deck.contextualCommands.length, 3);
        compare(findChild(deck, "contextCommand_repair"), null);
    }
}
