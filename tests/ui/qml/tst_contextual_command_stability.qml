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
            "detail": ({})
        };
    }

    function test_refresh_preserves_buttons_when_membership_is_unchanged() {
        var deck = commandDeck.createObject(testCase);
        verify(deck !== null);
        deck.action_states = ({"build": state(1, 0), "collect": state(1, 0)});
        compare(deck.contextualCommands.length, 2);
        var first = deck.contextualCommands;
        deck.action_states = ({"build": state(1, 0), "collect": state(1, 1)});
        verify(deck.contextualCommands === first,
               "A new model destroys buttons between mouse press and release");
        deck.destroy();
    }

    function test_a_contextual_click_survives_a_state_refresh() {
        var deck = commandDeck.createObject(testCase);
        verify(deck !== null);
        deck.action_states = ({"deliver": state(1, 0)});
        waitForRendering(deck);
        var button = findChild(deck, "contextCommand_deliver");
        verify(button !== null, "the contextual command was not created");
        verify(button.interactive);
        var spy = signalSpy.createObject(testCase, {
                "target": deck,
                "signalName": "command_mode_changed"
            });
        verify(spy !== null);
        mousePress(button, button.width / 2, button.height / 2);
        wait(150); // Longer than the HUD's 100 ms refresh period.
        deck.action_states = ({"deliver": state(1, 0)});
        mouseRelease(button, button.width / 2, button.height / 2);
        compare(spy.count, 1, "refresh must not swallow a contextual command click");
        spy.destroy();
        deck.destroy();
    }

    function test_membership_changes_still_refresh_the_deck() {
        var deck = commandDeck.createObject(testCase);
        verify(deck !== null);
        deck.action_states = ({"build": state(1, 0)});
        var initial = deck.contextualCommands;
        deck.action_states = ({"build": state(1, 0), "repair": state(1, 0)});
        verify(deck.contextualCommands !== initial);
        compare(deck.contextualCommands.length, 2);
        deck.destroy();
    }
}
