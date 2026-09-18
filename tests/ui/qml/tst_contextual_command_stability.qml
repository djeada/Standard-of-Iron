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
