import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    id: productionPanel

    property int selection_tick: 0
    property var production: null
    property var placement: null
    property var player_state: null
    readonly property var hs: StyleGuide.historical
    readonly property bool has_barracks_selection: (productionPanel.selection_tick, productionPanel.has_selected_type("barracks"))
    readonly property var barracks_state: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_state) ? productionPanel.production.selected_state() : productionPanel.default_production_state())
    readonly property var barracks_ally_call: (productionPanel.selection_tick, (productionPanel.has_barracks_selection && productionPanel.production && productionPanel.production.ally_call_state) ? productionPanel.production.ally_call_state(productionPanel.production.selected_building_id()) : ({}))

    signal recruit_unit(string unit_type)
    signal rally_mode_toggled
    signal build_tower
    signal builder_construction(string item_type)

    function has_selected_type(type) {
        return !!(productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type(type));
    }

    readonly property var recruit_unit_cards: [{
            "unit_type": "archer",
            "fallback_name": "Archer",
            "build_time": 5
        }, {
            "unit_type": "swordsman",
            "fallback_name": "Swordsman",
            "build_time": 7
        }, {
            "unit_type": "spearman",
            "fallback_name": "Spearman",
            "build_time": 6
        }, {
            "unit_type": "horse_swordsman",
            "fallback_name": qsTr("Mounted Knight"),
            "build_time": 10
        }, {
            "unit_type": "horse_archer",
            "fallback_name": qsTr("Horse Archer"),
            "build_time": 9
        }, {
            "unit_type": "horse_spearman",
            "fallback_name": qsTr("Horse Spearman"),
            "build_time": 9
        }, {
            "unit_type": "builder",
            "fallback_name": "Builder",
            "build_time": 6
        }, {
            "unit_type": "elephant",
            "fallback_name": qsTr("War Elephant"),
            "build_time": 20,
            "carthage_only": true
        }]

    readonly property var temple_recruit_cards: [{
            "unit_type": "healer",
            "fallback_name": "Healer",
            "build_time": 8
        }]

    signal unit_details_requested(string unit_type, string nation)

    function recruit_tooltip(unitInfo, fallbackName, fallbackTime, carthageOnly) {
        var name = (unitInfo && unitInfo.display_name) || fallbackName;
        var cost = productionPanel.format_cost_summary(productionPanel.reserve_cost(unitInfo), (unitInfo && unitInfo.resource_costs) || {}, qsTr("reserve"));
        var time = ((unitInfo && unitInfo.build_time) || fallbackTime).toFixed(0);
        if (carthageOnly)
            return qsTr("Recruit %1\nCost: %2\nBuild time: %3s\nCarthage exclusive").arg(name).arg(cost).arg(time);
        return qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(name).arg(cost).arg(time);
    }
    readonly property var marketplace_trade_specs: [{
            "key": "food",
            "label": qsTr("Food")
        }, {
            "key": "wood",
            "label": qsTr("Wood")
        }, {
            "key": "stone",
            "label": qsTr("Stone")
        }, {
            "key": "iron",
            "label": qsTr("Iron")
        }]
    readonly property var builder_card_specs: [{
            "item_type": "catapult",
            "label": qsTr("Catapult"),
            "description": qsTr("Long-range siege weapon\nEffective against structures"),
            "fallback_emoji": ""
        }, {
            "item_type": "ballista",
            "label": qsTr("Ballista"),
            "description": qsTr("Precision siege weapon\nEffective against units"),
            "fallback_emoji": ""
        }, {
            "item_type": "ram",
            "label": qsTr("Battering Ram"),
            "description": qsTr("Gate-breaking siege engine\nImmune to most arrows"),
            "fallback_emoji": ""
        }, {
            "item_type": "siege_tower",
            "label": qsTr("Siege Tower"),
            "description": qsTr("Docks against an enemy wall\nInfantry beside it climb up and cross"),
            "fallback_emoji": ""
        }, {
            "item_type": "defense_tower",
            "label": qsTr("Defense Tower"),
            "description": qsTr("Stationary defense structure\nShoots arrows at enemies"),
            "fallback_emoji": Design.Icons.unitGlyph("defense_tower")
        }, {
            "item_type": "home",
            "label": qsTr("Home"),
            "description": qsTr("Residential building\nAdds +50 reserve to the nearest barracks"),
            "fallback_emoji": Design.Icons.unitGlyph("home")
        }, {
            "item_type": "marketplace",
            "label": qsTr("Marketplace"),
            "description": qsTr("Trade building\nBuy or sell resources for gold"),
            "fallback_emoji": Design.Icons.unitGlyph("marketplace")
        }, {
            "item_type": "wall_segment",
            "label": qsTr("Wall Segment"),
            "description": qsTr("Wooden defensive wall\nBlocks enemy movement"),
            "fallback_emoji": Design.Icons.collect
        }, {
            "item_type": "wall_gate",
            "label": qsTr("Wall Gate"),
            "description": qsTr("Gated opening in a wall\nOpens for your troops and allies"),
            "fallback_emoji": Design.Icons.gate
        }, {
            "item_type": "wall_ladder",
            "label": qsTr("Ladder"),
            "description": qsTr("Leans on the town side of your wall\nAnother way up onto the wall walk"),
            "fallback_emoji": Design.Icons.unitGlyph("wall_ladder")
        }]

    function default_production_state() {
        return {
            "has_barracks": false,
            "produced_count": 0,
            "max_units": 0,
            "queue_size": 0,
            "in_progress": false,
            "production_queue": [],
            "product_type": "",
            "villager_cost": 1,
            "manpower_available": 0,
            "build_time": 0,
            "time_remaining": 0,
            "nation_id": "",
            "has_home": false,
            "has_temple": false
        };
    }

    function default_marketplace_state() {
        return {
            "has_marketplace": false,
            "nation_id": "",
            "trade_quantity": 0,
            "buy_prices": {},
            "sell_prices": {}
        };
    }

    function unit_icon_source(unit_type, nation_key) {
        return Design.Icons.unit(unit_type, nation_key);
    }

    function unit_icon_emoji(unit_type) {
        if (productionPanel.is_commander_type(unit_type))
            return Design.Icons.commander;
        return Design.Icons.unitGlyph(unit_type);
    }

    function is_commander_type(unit_type) {
        if (!unit_type)
            return false;
        return unit_type.indexOf("commander") !== -1 || unit_type === "roman_legion_organizer" || unit_type === "roman_veteran_consul" || unit_type === "carthage_spear_commander" || unit_type === "carthage_bow_commander" || unit_type === "carthage_sword_commander";
    }

    function get_unit_production_info(unit_type, nation_id) {
        if (productionPanel.production && productionPanel.production.unit_info)
            return productionPanel.production.unit_info(unit_type, nation_id || "");
        return {
            "cost": 50,
            "resource_costs": {},
            "build_time": 5,
            "individuals_per_unit": 1,
            "display_name": unit_type
        };
    }

    function get_construction_info(item_type) {
        if (productionPanel.production && productionPanel.placement)
            return productionPanel.placement.get_construction_info(item_type || "");
        return {
            "build_time": 10,
            "resource_costs": {},
            "display_name": item_type
        };
    }

    function current_resources() {
        if (productionPanel.production && productionPanel.player_state && productionPanel.player_state.resources)
            return productionPanel.player_state.resources;
        return {};
    }

    function reserve_cost(info) {
        if (!info)
            return 0;
        return Math.max(0, info.cost || 0);
    }

    function resource_amount(costs, key) {
        if (!costs || costs[key] === undefined)
            return 0;
        return Math.max(0, costs[key] || 0);
    }

    function missing_resource_amounts(costs) {
        var resources = current_resources();
        var missing = {};
        for (var i = 0; i < EconomyGuide.resourceOrder.length; ++i) {
            var key = EconomyGuide.resourceOrder[i];
            var shortfall = resource_amount(costs, key) - resource_amount(resources, key);
            if (shortfall > 0)
                missing[key] = shortfall;
        }
        return missing;
    }

    function can_afford_resource_costs(costs) {
        return Object.keys(missing_resource_amounts(costs)).length === 0;
    }

    function missing_resource_reason(costs) {
        return qsTr("Need %1").arg(EconomyGuide.missing_summary(missing_resource_amounts(costs)));
    }

    function cost_entries(reserveCost, resourceCosts, includeReserve) {
        var entries = [];
        if (includeReserve && reserveCost > 0)
            entries.push({
                    "key": "reserve",
                    "amount": reserveCost
                });
        var ordered = EconomyGuide.resourceOrder;
        for (var i = 0; i < ordered.length; ++i) {
            var key = ordered[i];
            var amount = resource_amount(resourceCosts, key);
            if (amount > 0)
                entries.push({
                        "key": key,
                        "amount": amount
                    });
        }
        return entries;
    }

    function cost_icon_source(key) {
        if (key === "reserve")
            return StyleGuide.icon_path("troop_count.png");
        return StyleGuide.icon_path(key + ".png");
    }

    function format_cost_summary(reserveCost, resourceCosts, reserveLabel) {
        var parts = [];
        if (reserveCost > 0)
            parts.push(qsTr("%1 %2").arg(reserveCost).arg(reserveLabel));
        for (var i = 0; i < EconomyGuide.resourceOrder.length; ++i) {
            var key = EconomyGuide.resourceOrder[i];
            var amount = resource_amount(resourceCosts, key);
            if (amount > 0)
                parts.push(qsTr("%1 %2").arg(amount).arg(EconomyGuide.resource_label(key)));
        }
        return parts.join(", ");
    }

    function trade_price(priceMap, key) {
        if (!priceMap || priceMap[key] === undefined)
            return 0;
        return Math.max(0, priceMap[key] || 0);
    }

    function can_buy_trade_resource(marketState, key) {
        if (!marketState || !marketState.has_marketplace)
            return false;
        return resource_amount(current_resources(), "gold") >= trade_price(marketState.buy_prices, key);
    }

    function can_sell_trade_resource(marketState, key) {
        if (!marketState || !marketState.has_marketplace)
            return false;
        return resource_amount(current_resources(), key) >= Math.max(0, marketState.trade_quantity || 0);
    }

    function recruit_card_state(prod, unitInfo, queueTotal) {
        var reserveCost = reserve_cost(unitInfo);
        if (!(prod.has_barracks || prod.has_home || prod.has_temple))
            return {
                "enabled": false,
                "reason": qsTr("Cannot recruit")
            };
        if (queueTotal >= 5)
            return {
                "enabled": false,
                "reason": qsTr("Queue is full (5/5)")
            };
        if ((prod.manpower_available || 0) < reserveCost)
            return {
                "enabled": false,
                "reason": qsTr("Not enough reserve")
            };
        if (!can_afford_resource_costs(unitInfo.resource_costs || {}))
            return {
                "enabled": false,
                "reason": missing_resource_reason(unitInfo.resource_costs || {})
            };
        return {
            "enabled": true,
            "reason": ""
        };
    }

    function construction_card_state(builderProd, constructionInfo) {
        if (builderProd.in_progress && !builderProd.gathering)
            return {
                "enabled": false,
                "reason": qsTr("Already building...")
            };
        if (!can_afford_resource_costs(constructionInfo.resource_costs || {}))
            return {
                "enabled": false,
                "reason": missing_resource_reason(constructionInfo.resource_costs || {})
            };
        return {
            "enabled": true,
            "reason": ""
        };
    }

    function meter_color(ratio) {
        if (ratio > 0.6)
            return Theme.accent;
        if (ratio > 0.3)
            return hs.bronze;
        return hs.waxHover;
    }

    function recruit_card_color(enabled, hovered) {
        if (!enabled)
            return Theme.bgShade;
        return hovered ? Qt.lighter(hs.parchmentLight, 1.3) : hs.parchmentLight;
    }

    function recruit_card_border(enabled, hovered) {
        if (!enabled)
            return hs.parchmentLight;
        return hovered ? hs.bronze : hs.bronzeDeep;
    }

    color: hs.parchmentDark
    border.color: hs.bronze
    border.width: 2
    radius: 6
    clip: true

    BarracksProductionView {
        id: barracksView

        objectName: "barracksProductionView"
        anchors.fill: parent
        anchors.margins: Design.Metrics.space8
        visible: productionPanel.has_barracks_selection
        panel: productionPanel
        prod: productionPanel.barracks_state
        allyCall: productionPanel.barracks_ally_call

        onRecruit_requested: function (unitType) {
            productionPanel.recruit_unit(unitType);
        }
        onDetails_requested: function (unitType, nation) {
            productionPanel.unit_details_requested(unitType, nation);
        }
        onRally_requested: productionPanel.rally_mode_toggled()
        onAlly_call_requested: {
            if (productionPanel.production && productionPanel.production.call_allies)
                productionPanel.production.call_allies(productionPanel.production.selected_building_id());
        }
    }

    Flickable {
        id: productionScroll

        readonly property int gutter: Design.Metrics.scrollBarThickness + Design.Metrics.space4

        anchors.fill: parent
        anchors.margins: Design.Metrics.space8
        visible: !productionPanel.has_barracks_selection
        enabled: visible
        clip: true
        contentWidth: width
        contentHeight: productionColumn.implicitHeight
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Design.IronScrollBar {
            objectName: "productionScrollBar"
        }

        Column {
            id: productionColumn

            width: productionScroll.width - productionScroll.gutter
            spacing: Design.Metrics.space4

            ProductionBarracksQueue {
                panel: productionPanel
            }

            ProductionBarracksRoster {
                panel: productionPanel
            }

            ProductionHomeSection {
                panel: productionPanel
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))

                width: parent.width
                height: 1
                color: "#3B2F24"
                visible: has_barracks || (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("home"))
            }

            ProductionRallySection {
                panel: productionPanel
            }

            Item {
                property bool has_barracks_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))
                property bool has_home_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("home")))

                height: 20
                visible: !has_barracks_selected && !has_home_selected
            }

            ProductionBuilderSection {
                panel: productionPanel
            }

            ProductionMarketplaceSection {
                panel: productionPanel
            }

            ProductionTempleSection {
                panel: productionPanel
            }

            ProductionFarmSection {
                panel: productionPanel
            }

            ProductionEmptyHint {
                panel: productionPanel
                viewportHeight: productionScroll.height
            }
        }
    }
}
