import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import StandardOfIron.Core 1.0 as Core
import "../../../ui/qml"

TestCase {
    id: testCase

    property var calls: []

    name: "MarketplacePanel"
    when: windowShown
    width: 800
    height: 400
    visible: true

    function init() {
        Core.UiPreferences.reset_to_defaults();
        testCase.calls = [];
        market.allies = [];
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    QtObject {
        id: market

        function has_selected_type(type) {
            return type === "marketplace";
        }

        function selected_marketplace_state() {
            return {
                "has_marketplace": true,
                "nation_id": "roman_republic",
                "trade_quantity": 10,
                "buy_prices": {
                    "food": 10,
                    "wood": 12,
                    "stone": 15,
                    "iron": 20
                },
                "sell_prices": {
                    "food": 5,
                    "wood": 6,
                    "stone": 8,
                    "iron": 12
                }
            };
        }

        function marketplace_buy(key) {
            testCase.calls.push("buy " + key);
            return true;
        }

        function marketplace_sell(key) {
            testCase.calls.push("sell " + key);
            return true;
        }

        property var allies: []

        function marketplace_allies() {
            return market.allies;
        }

        function send_to_ally(owner, key, amount) {
            testCase.calls.push("send " + owner + " " + key + " " + amount);
            return true;
        }

        function request_from_ally(owner, key, amount) {
            testCase.calls.push("request " + owner + " " + key + " " + amount);
            return true;
        }
    }

    QtObject {
        id: player

        property var resources: ({
                "food": 600,
                "wood": 5,
                "stone": 600,
                "iron": 300,
                "gold": 2000
            })
    }

    Component {
        id: panelComponent

        ProductionPanel {
        }
    }

    function create_panel() {
        var panel = createTemporaryObject(panelComponent, testCase, {
                "width": 410,
                "height": 170,
                "production": market,
                "player_state": player
            });
        verify(panel !== null);
        panel.selection_tick = 1;
        waitForRendering(panel);
        return panel;
    }

    function test_every_resource_fits_in_the_hud_zone() {
        var panel = create_panel();
        var keys = ["food", "wood", "stone", "iron"];
        for (var i = 0; i < keys.length; ++i) {
            var row = findChild(panel, "marketplaceRow_" + keys[i]);
            verify(row !== null && row.visible, keys[i] + " row must be shown");
            var at = row.mapToItem(panel, 0, 0);
            verify(at.y >= 0 && at.y + row.height <= panel.height, keys[i] + " row must be on screen without scrolling (" + at.y + "+" + row.height + ")");
        }
    }

    function test_trades_reach_the_view_model() {
        var panel = create_panel();
        mouseClick(findChild(panel, "marketplaceBuy_stone"));
        mouseClick(findChild(panel, "marketplaceSell_iron"));
        compare(testCase.calls, ["buy stone", "sell iron"]);
    }

    function test_a_trade_the_player_cannot_afford_is_refused() {
        var panel = create_panel();
        var sell_wood = findChild(panel, "marketplaceSell_wood");
        verify(!sell_wood.allowed, "five wood cannot fill a lot of ten");
        mouseClick(sell_wood);
        compare(testCase.calls, []);
    }

    function test_without_allies_the_exchange_says_so() {
        var panel = create_panel();
        var exchange = findChild(panel, "allyExchange");
        verify(exchange !== null && exchange.visible);
        verify(findChild(panel, "allySendButton").visible === false);
    }

    function test_resources_are_sent_to_and_requested_from_the_chosen_ally() {
        market.allies = [{
                "owner_id": 3,
                "name": "Hanno",
                "is_ai": true
            }, {
                "owner_id": 4,
                "name": "Mago",
                "is_ai": true
            }];
        var panel = create_panel();
        var exchange = findChild(panel, "allyExchange");
        exchange.ally_index = 1;
        exchange.resource_key = "gold";
        exchange.amount = 100;
        findChild(panel, "allySendButton").clicked();
        findChild(panel, "allyRequestButton").clicked();
        compare(testCase.calls, ["send 4 gold 100", "request 4 gold 100"]);
    }

    function test_sending_more_than_the_player_holds_is_disabled() {
        market.allies = [{
                "owner_id": 3,
                "name": "Hanno",
                "is_ai": true
            }];
        var panel = create_panel();
        var exchange = findChild(panel, "allyExchange");
        exchange.resource_key = "wood";
        exchange.amount = 50;
        verify(!findChild(panel, "allySendButton").enabled, "five wood cannot make a gift of fifty");
    }
}
