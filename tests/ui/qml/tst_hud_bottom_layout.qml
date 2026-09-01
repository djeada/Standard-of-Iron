import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "HudBottomLayout"
    when: windowShown
    width: 1280
    height: Metrics.bottomBarHeight(720, false)
    visible: true

    property var panel: null

    function init() {
        Core.UiPreferences.reset_to_defaults();
        panel = hudComponent.createObject(testCase);
        verify(panel !== null, "the bottom HUD was not created");
        wait(1);
    }

    function cleanup() {
        if (panel) {
            panel.destroy();
            panel = null;
        }
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function collect(item, predicate, found) {
        var out = found || [];
        if (!item)
            return out;
        var kids = item.children;
        for (var i = 0; i < kids.length; ++i) {
            if (predicate(kids[i]))
                out.push(kids[i]);
            collect(kids[i], predicate, out);
        }
        return out;
    }

    function named(name) {
        var found = collect(panel, function (item) {
                return item.objectName === name;
            });
        compare(found.length, 1, "expected one " + name);
        return found[0];
    }

    function state(eligible) {
        return {
            "enabled": eligible > 0,
            "active": false,
            "mixed": false,
            "placing": false,
            "passive": false,
            "eligibleCount": eligible,
            "activeCount": 0,
            "readyCount": eligible,
            "detail": ({})
        };
    }

    function test_the_three_zones_are_equally_wide_at_720p() {
        var selection = named("selectionZone");
        var commands = named("commandDeck");
        var production = named("productionZone");
        var expected = (panel.width - panel.spacing * 2) / 3;
        var zones = [selection, commands, production];
        for (var i = 0; i < zones.length; ++i)
            verify(Math.abs(zones[i].width - expected) <= 1, zones[i].objectName + " is not an equal third: " + zones[i].width + " != " + expected);
    }

    function test_every_command_card_has_the_same_dimensions() {
        panel.action_states = {
            "build": state(1),
            "collect": state(1),
            "auto_gather": state(1)
        };
        wait(1);
        var cards = collect(panel, function (item) {
                var name = String(item.objectName);
                return name.indexOf("primaryCommand_") === 0 || name.indexOf("contextCommand_") === 0;
            });
        compare(cards.length, 8, "expected the five primary orders plus three builder actions");
        for (var i = 1; i < cards.length; ++i) {
            compare(cards[i].width, cards[0].width, cards[i].objectName + " is a different width from " + cards[0].objectName);
            compare(cards[i].height, cards[0].height, cards[i].objectName + " is a different height from " + cards[0].objectName);
        }
    }

    function test_primary_orders_are_labelled_full_size_and_never_scrolled() {
        var buttons = collect(panel, function (item) {
                return String(item.objectName).indexOf("primaryCommand_") === 0;
            });
        compare(buttons.length, 5, "the primary row must always expose five core orders");
        for (var i = 0; i < buttons.length; ++i) {
            compare(buttons[i].iconOnly, false, buttons[i].objectName + " fell back to an icon-only contract");
            verify(buttons[i].height >= Metrics.commandButtonSize - 1, buttons[i].objectName + " is too short: " + buttons[i].height);
            verify(buttons[i].width > Metrics.orderButtonSize, buttons[i].objectName + " still has the old tiny tile width: " + buttons[i].width);
        }
        compare(collect(panel, function (item) {
                    return item.objectName === "orderScrollView";
                }).length, 0, "the primary command deck still relies on horizontal scrolling");
        verify(named("commandModeBanner").height >= Metrics.controlHeight, "the current mode banner is still a caption strip");
    }

    function test_builder_actions_fill_the_context_row_without_hiding_primary_orders() {
        panel.action_states = {
            "build": state(1),
            "collect": state(1),
            "auto_gather": state(1),
            "repair": state(1),
            "dismantle": state(1)
        };
        wait(1);
        var contextButtons = collect(panel, function (item) {
                return String(item.objectName).indexOf("contextCommand_") === 0;
            });
        compare(contextButtons.length, 5, "a builder should expose its five specialist actions");
        for (var i = 0; i < contextButtons.length; ++i) {
            verify(contextButtons[i].width >= Metrics.minTouchTarget, contextButtons[i].objectName + " became too narrow");
            verify(contextButtons[i].height >= Metrics.commandButtonSize - 1, contextButtons[i].objectName + " became too short");
        }
        compare(collect(panel, function (item) {
                    return String(item.objectName).indexOf("primaryCommand_") === 0;
                }).length, 5, "context actions displaced the primary row");
    }

    Component {
        id: hudComponent

        HUDBottom {
            anchors.fill: parent
        }
    }
}
