import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: productionPanel

    property int selection_tick: 0
    property var production: null
    property var placement: null
    property var player_state: null
    readonly property var hs: StyleGuide.historical

    signal recruit_unit(string unit_type)
    signal rally_mode_toggled
    signal build_tower
    signal builder_construction(string item_type)

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
        var cost = productionPanel.format_cost_summary(productionPanel.population_cost(unitInfo), (unitInfo && unitInfo.resource_costs) || {}, qsTr("population"));
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
            "item_type": "defense_tower",
            "label": qsTr("Defense Tower"),
            "description": qsTr("Stationary defense structure\nShoots arrows at enemies"),
            "fallback_emoji": Design.Icons.unitGlyph("defense_tower")
        }, {
            "item_type": "home",
            "label": qsTr("Home"),
            "description": qsTr("Residential building\nAdds +50 population to nearest barracks"),
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
            "population_cost": 50,
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

    function population_cost(info) {
        if (!info)
            return 0;
        if (info.population_cost !== undefined)
            return Math.max(0, info.population_cost || 0);
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

    function cost_entries(popCost, resourceCosts, includePopulation) {
        var entries = [];
        if (includePopulation && popCost > 0)
            entries.push({
                    "key": "population",
                    "amount": popCost
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
        if (key === "population")
            return StyleGuide.icon_path("troop_count.png");
        return StyleGuide.icon_path(key + ".png");
    }

    function format_cost_summary(popCost, resourceCosts, populationLabel) {
        var parts = [];
        if (popCost > 0)
            parts.push(qsTr("%1 %2").arg(popCost).arg(populationLabel));
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
        var popCost = population_cost(unitInfo);
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
        if ((prod.manpower_available || 0) < popCost)
            return {
                "enabled": false,
                "reason": qsTr("Not enough available population")
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
        if (builderProd.in_progress)
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
        return hovered ? hs.bannerNeutral : hs.parchmentDark;
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

    ScrollView {
        anchors.fill: parent
        anchors.margins: 10
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Column {
            width: productionPanel.width - 20
            spacing: 8

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))

                width: parent.width
                height: productionContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: productionContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_state) ? productionPanel.production.selected_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 10
                    width: parent.width - 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("PRODUCTION QUEUE")
                        color: hs.bronze
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Repeater {
                            model: 5

                            Rectangle {
                                property int queue_total: (productionContent.prod.in_progress ? 1 : 0) + (productionContent.prod.queue_size || 0)
                                property bool is_occupied: index < queue_total
                                property bool is_producing: index === 0 && productionContent.prod.in_progress
                                property string queue_unit_type: {
                                    if (!is_occupied)
                                        return "";
                                    if (index === 0 && productionContent.prod.in_progress)
                                        return productionContent.prod.product_type || "archer";
                                    var queueIndex = productionContent.prod.in_progress ? index - 1 : index;
                                    if (productionContent.prod.production_queue && productionContent.prod.production_queue[queueIndex])
                                        return productionContent.prod.production_queue[queueIndex];
                                    return "archer";
                                }

                                width: Design.A11y.scaled(36)
                                height: Design.A11y.scaled(36)
                                radius: 6
                                color: is_producing ? "#7F9A5F" : (is_occupied ? "#2F251D" : "#120D09")
                                border.color: is_producing ? "#8FA46B" : (is_occupied ? "#6F8E8C" : "#3B2F24")
                                border.width: 2

                                Image {
                                    id: queueIconImage

                                    anchors.centerIn: parent
                                    width: Design.A11y.scaled(28)
                                    height: Design.A11y.scaled(28)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    source: parent.is_occupied ? productionPanel.unit_icon_source(parent.queue_unit_type, productionContent.prod.nation_id) : ""
                                    visible: parent.is_occupied && source !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.is_occupied ? productionPanel.unit_icon_emoji(parent.queue_unit_type) : "·"
                                    color: parent.is_producing ? "#F4E7C8" : (parent.is_occupied ? "#D4B57C" : "#6B5231")
                                    font.pixelSize: parent.is_occupied ? Design.Typography.subheading : Design.Typography.heading
                                    font.bold: parent.is_producing
                                    visible: !queueIconImage.visible
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    text: (index + 1).toString()
                                    color: parent.is_occupied ? "#8D7146" : "#3B2F24"
                                    font.pixelSize: Design.Typography.caption
                                    font.bold: true
                                }

                                SequentialAnimation on opacity  {
                                    running: is_producing
                                    loops: Animation.Infinite

                                    NumberAnimation {
                                        from: 0.7
                                        to: 1
                                        duration: 800
                                    }

                                    NumberAnimation {
                                        from: 1
                                        to: 0.7
                                        duration: 800
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        property int queue_total: (productionContent.prod.in_progress ? 1 : 0) + (productionContent.prod.queue_size || 0)

                        anchors.horizontalCenter: parent.horizontalCenter
                        text: queue_total + " / 5"
                        color: queue_total >= 5 ? "#C0403B" : "#D4B57C"
                        font.pixelSize: Design.Typography.caption
                        font.bold: queue_total >= 5
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: Math.max(Design.A11y.scaled(20), Design.Typography.label + 6)
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#120D09"
                        border.color: "#2F251D"
                        border.width: 2
                        visible: productionContent.prod.in_progress

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: {
                                if (!productionContent.prod.in_progress || productionContent.prod.build_time <= 0)
                                    return 0;
                                var progress = 1 - (Math.max(0, productionContent.prod.time_remaining) / productionContent.prod.build_time);
                                return Math.max(0, (parent.width - 4) * progress);
                            }
                            color: "#7F9A5F"
                            radius: 8

                            SequentialAnimation on opacity  {
                                running: parent.width > 0
                                loops: Animation.Infinite

                                NumberAnimation {
                                    from: 0.8
                                    to: 1
                                    duration: 600
                                }

                                NumberAnimation {
                                    from: 1
                                    to: 0.8
                                    duration: 600
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: productionContent.prod.in_progress ? qsTr("%1s").arg(Math.max(0, productionContent.prod.time_remaining).toFixed(1)) : qsTr("Idle")
                            color: "#F4E7C8"
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Available Population: %1 / %2").arg(productionContent.prod.manpower_available || 0).arg(productionContent.prod.max_units || 0)
                        color: (productionContent.prod.manpower_available <= 0) ? "#C0403B" : "#D4B57C"
                        font.pixelSize: Design.Typography.caption
                    }
                }
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))

                width: parent.width
                height: unitGridContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: unitGridContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_state) ? productionPanel.production.selected_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("RECRUIT UNITS")
                        color: hs.bronze
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Repeater {
                            model: productionPanel.recruit_unit_cards

                            delegate: RecruitCard {
                                required property var modelData

                                panel: productionPanel
                                prod: unitGridContent.prod
                                unit_type: modelData.unit_type
                                fallback_build_time: modelData.build_time
                                tooltip_text: panel ? panel.recruit_tooltip(unit_info, modelData.fallback_name, modelData.build_time, modelData.carthage_only === true) : ""
                                visible: modelData.carthage_only !== true || unitGridContent.prod.nation_id === "carthage"
                                onRecruit_requested: function (unitType) {
                                    productionPanel.recruit_unit(unitType);
                                }
                                onDetails_requested: function (unitType, nation) {
                                    productionPanel.unit_details_requested(unitType, nation);
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                property bool has_home: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("home")))

                width: parent.width
                height: homeProductionContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_home

                Column {
                    id: homeProductionContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_home_state) ? productionPanel.production.selected_home_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("HOME RECRUITMENT")
                        color: hs.bronze
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: {
                            var info = productionPanel.get_unit_production_info("civilian", homeProductionContent.prod.nation_id);
                            var cost = Math.max(1, info.cost || 1);
                            var ready = Math.floor((homeProductionContent.prod.manpower_available || 0) / cost);
                            return qsTr("Available civilians: %1 / %2").arg(ready).arg(homeProductionContent.prod.max_units || 0);
                        }
                        color: Theme.textSubLite
                        font.pixelSize: Design.Typography.caption
                    }

                    Rectangle {
                        id: civilianCard

                        property int queue_total: (homeProductionContent.prod.in_progress ? 1 : 0) + (homeProductionContent.prod.queue_size || 0)
                        property var unit_info: productionPanel.get_unit_production_info("civilian", homeProductionContent.prod.nation_id)
                        property int committed_total: (homeProductionContent.prod.produced_count || 0) + queue_total
                        property bool has_capacity: committed_total < (homeProductionContent.prod.max_units || 0)
                        property bool has_families: (homeProductionContent.prod.manpower_available || 0) >= productionPanel.population_cost(unit_info)
                        property var recruit_state: productionPanel.recruit_card_state(homeProductionContent.prod, unit_info, queue_total)
                        property bool is_enabled: homeProductionContent.prod.has_home && has_capacity && recruit_state.enabled
                        property bool is_hovered: civilianMouseArea.containsMouse

                        width: 110
                        height: 80
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 6
                        color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                        border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                        border.width: is_hovered && is_enabled ? 2 : 1
                        opacity: is_enabled ? 1 : 0.5
                        scale: is_hovered && is_enabled ? 1.025 : 1

                        Image {
                            id: civilianRecruitIcon

                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                            source: productionPanel.unit_icon_source("civilian", homeProductionContent.prod.nation_id)
                            visible: source !== ""
                            opacity: parent.is_enabled ? 1 : 0.35
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !civilianRecruitIcon.visible
                            text: productionPanel.unit_icon_emoji("civilian")
                            color: parent.is_enabled ? Theme.textMain : Theme.textHint
                            font.pixelSize: Design.Typography.glyph
                            opacity: parent.is_enabled ? 0.9 : 0.4
                        }

                        Flow {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            spacing: 4

                            Repeater {
                                model: productionPanel.cost_entries(productionPanel.population_cost(civilianCard.unit_info), civilianCard.unit_info.resource_costs || {}, true)

                                delegate: Rectangle {
                                    width: civilianCostRow.implicitWidth + 8
                                    height: civilianCostRow.implicitHeight + 6
                                    radius: 8
                                    color: civilianCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                    border.color: civilianCard.is_enabled ? hs.bronze : "#8C6A3E"
                                    border.width: 1

                                    Row {
                                        id: civilianCostRow

                                        anchors.centerIn: parent
                                        spacing: 3

                                        Image {
                                            width: Design.A11y.scaled(9)
                                            height: Design.A11y.scaled(9)
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            source: productionPanel.cost_icon_source(modelData.key)
                                        }

                                        Text {
                                            text: modelData.amount
                                            color: civilianCard.is_enabled ? Theme.textMain : Theme.textDim
                                            font.pixelSize: Design.Typography.caption
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: civilianMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (parent.is_enabled) {
                                    Design.UiSound.activate();
                                    productionPanel.recruit_unit("civilian");
                                } else {
                                    Design.UiSound.warning();
                                }
                            }
                            onContainsMouseChanged: {
                                if (containsMouse && parent.is_enabled)
                                    Design.UiSound.hover();
                            }
                            cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s\nUse Deliver mode, then click a friendly barracks to add 50 available population.").arg(parent.unit_info.display_name || "Civilian").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("families"))).arg((parent.unit_info.build_time || 5).toFixed(0)) : (!civilianCard.has_capacity ? qsTr("This home already committed its 3 civilians") : civilianCard.recruit_state.reason)
                            ToolTip.delay: 300
                        }
                    }
                }
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))

                width: parent.width
                height: 1
                color: "#3B2F24"
                visible: has_barracks || (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("home"))
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && (productionPanel.production.has_selected_type("barracks") || productionPanel.production.has_selected_type("temple"))))

                width: parent.width
                height: rallyContent.height + 12
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: rallyContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_state) ? productionPanel.production.selected_state() : productionPanel.default_production_state())
                    property var temple_prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_temple_state) ? productionPanel.production.selected_temple_state() : productionPanel.default_production_state())
                    property bool placing_barracks_rally: typeof gameView !== 'undefined' && gameView.cursor_mode === "place_barracks_rally"

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 6

                    Button {
                        id: rallyButton

                        readonly property bool allowed: rallyContent.prod.has_barracks || rallyContent.temple_prod.has_temple

                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.parent.width - 20
                        height: Design.A11y.scaled(32)
                        text: rallyContent.placing_barracks_rally ? Design.Icons.rally + " " + qsTr("Click Map to Set Rally") : Design.Icons.rally + " " + qsTr("Set Rally Point")
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            if (!allowed) {
                                Design.UiSound.warning();
                                return;
                            }
                            Design.UiSound.activate();
                            productionPanel.rally_mode_toggled();
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: allowed ? qsTr("Set where newly recruited units will gather.\nRight-click to cancel.") : qsTr("Select a barracks or temple before setting a rally point.")
                        ToolTip.delay: 500

                        background: Rectangle {
                            color: rallyButton.allowed ? (parent.down ? hs.bronzeDeep : (parent.hovered ? hs.bronze : hs.parchmentDark)) : Theme.bgShade
                            radius: 6
                            border.color: rallyContent.placing_barracks_rally ? hs.bronze : hs.bronzeDeep
                            border.width: 2
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                            color: rallyButton.allowed ? "#F4E7C8" : "#6B5231"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: rallyContent.placing_barracks_rally ? qsTr("Right-click to cancel") : ""
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                        font.italic: true
                    }
                }
            }

            Item {
                property bool has_barracks_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))
                property bool has_home_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("home")))

                height: 20
                visible: !has_barracks_selected && !has_home_selected
            }

            Rectangle {
                property bool has_builder: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("builder")))

                width: parent.width
                height: builderProductionContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_builder

                Column {
                    id: builderProductionContent

                    property var builder_prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_builder_state) ? productionPanel.production.selected_builder_state() : {
                            "in_progress": false,
                            "build_time": 10,
                            "time_remaining": 0,
                            "product_type": ""
                        })

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Image {
                            id: builderHeaderIcon

                            width: 18
                            height: 18
                            source: productionPanel.unit_icon_source("builder")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: source !== ""
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: builderHeaderIcon.visible ? qsTr("BUILDER CONSTRUCTION") : Design.Icons.build + " " + qsTr("BUILDER CONSTRUCTION")
                            color: hs.bronze
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Build siege weapons, structures, and gather wood, stone, iron, and food")
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: Math.max(Design.A11y.scaled(20), Design.Typography.label + 6)
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#120D09"
                        border.color: "#2F251D"
                        border.width: 2
                        visible: builderProductionContent.builder_prod.in_progress

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: {
                                if (!builderProductionContent.builder_prod.in_progress || builderProductionContent.builder_prod.build_time <= 0)
                                    return 0;
                                var progress = 1 - (Math.max(0, builderProductionContent.builder_prod.time_remaining) / builderProductionContent.builder_prod.build_time);
                                return Math.max(0, (parent.width - 4) * progress);
                            }
                            color: "#7F9A5F"
                            radius: 8

                            SequentialAnimation on opacity  {
                                running: parent.width > 0
                                loops: Animation.Infinite

                                NumberAnimation {
                                    from: 0.8
                                    to: 1
                                    duration: 600
                                }

                                NumberAnimation {
                                    from: 1
                                    to: 0.8
                                    duration: 600
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: builderProductionContent.builder_prod.in_progress ? qsTr("%1s").arg(Math.max(0, builderProductionContent.builder_prod.time_remaining).toFixed(1)) : qsTr("Idle")
                            color: "#F4E7C8"
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: {
                            if (!builderProductionContent.builder_prod.in_progress)
                                return qsTr("Select a structure to build");
                            var label = builderProductionContent.builder_prod.product_type;
                            var is_collection_task = false;
                            if (label === "cut_tree") {
                                is_collection_task = true;
                                label = qsTr("Cut Tree");
                            } else if (label === "collect_stone") {
                                is_collection_task = true;
                                label = qsTr("Collect Stone");
                            } else if (label === "collect_iron_ore") {
                                is_collection_task = true;
                                label = qsTr("Collect Iron Ore");
                            } else if (label === "harvest_grain") {
                                is_collection_task = true;
                                label = qsTr("Harvest Grain");
                            } else if (label === "slaughter_sheep") {
                                is_collection_task = true;
                                label = qsTr("Slaughter Sheep");
                            } else if (label === "farm") {
                                label = qsTr("Farm");
                            } else if (label === "wall_segment") {
                                label = qsTr("Wall Segment");
                            } else if (label === "wall_gate") {
                                label = qsTr("Wall Gate");
                            }
                            return (is_collection_task ? qsTr("Task: %1") : qsTr("Building: %1")).arg(label);
                        }
                        color: builderProductionContent.builder_prod.in_progress ? "#7F9A5F" : "#8D7146"
                        font.pixelSize: Design.Typography.caption
                        font.bold: builderProductionContent.builder_prod.in_progress
                        visible: true
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Rectangle {
                            id: builderCatapultCard

                            property var construction_info: productionPanel.get_construction_info("catapult")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderCatapultMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderCatapultIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("catapult")
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderCatapultIcon.visible
                                text: productionPanel.unit_icon_emoji("catapult")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Catapult")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderCatapultCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: catapultCostRow.implicitWidth + 8
                                        height: catapultCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderCatapultCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderCatapultCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: catapultCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderCatapultCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderCatapultMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("catapult");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Catapult\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Long-range siege weapon\nEffective against structures")).arg(productionPanel.format_cost_summary(0, builderCatapultCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderCatapultCard.construction_info.build_time || 15).toFixed(0)) : builderCatapultCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderCatapultMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderBallistaCard

                            property var construction_info: productionPanel.get_construction_info("ballista")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderBallistaMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderBallistaIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("ballista")
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderBallistaIcon.visible
                                text: productionPanel.unit_icon_emoji("ballista")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Ballista")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderBallistaCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: ballistaCostRow.implicitWidth + 8
                                        height: ballistaCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderBallistaCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderBallistaCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: ballistaCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderBallistaCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderBallistaMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("ballista");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Ballista\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Precision siege weapon\nEffective against units")).arg(productionPanel.format_cost_summary(0, builderBallistaCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderBallistaCard.construction_info.build_time || 12).toFixed(0)) : builderBallistaCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderBallistaMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderDefenseTowerCard

                            property var construction_info: productionPanel.get_construction_info("defense_tower")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderDefenseTowerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderDefenseTowerIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("defense_tower")
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderDefenseTowerIcon.visible
                                text: Design.Icons.unitGlyph("defense_tower")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Defense Tower")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderDefenseTowerCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: defenseTowerCostRow.implicitWidth + 8
                                        height: defenseTowerCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderDefenseTowerCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderDefenseTowerCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: defenseTowerCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderDefenseTowerCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderDefenseTowerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("defense_tower");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Defense Tower\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Stationary defense structure\nShoots arrows at enemies")).arg(productionPanel.format_cost_summary(0, builderDefenseTowerCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderDefenseTowerCard.construction_info.build_time || 20).toFixed(0)) : builderDefenseTowerCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderDefenseTowerMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderHomeCard

                            property var construction_info: productionPanel.get_construction_info("home")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderHomeMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderHomeIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("home")
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderHomeIcon.visible
                                text: Design.Icons.unitGlyph("home")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Home")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderHomeCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: homeCostRow.implicitWidth + 8
                                        height: homeCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderHomeCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderHomeCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: homeCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderHomeCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderHomeMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("home");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Home\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Residential building\nAdds +50 population to nearest barracks")).arg(productionPanel.format_cost_summary(0, builderHomeCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderHomeCard.construction_info.build_time || 10).toFixed(0)) : builderHomeCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderHomeMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderFarmCard

                            property var construction_info: productionPanel.get_construction_info("farm")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderFarmMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderFarmIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                source: productionPanel.unit_icon_source("farm")
                                visible: status === Image.Ready
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: builderFarmIcon.status !== Image.Ready
                                text: Design.Icons.unitGlyph("farm")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Farm")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderFarmCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: farmCostRow.implicitWidth + 8
                                        height: farmCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderFarmCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderFarmCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: farmCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderFarmCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderFarmMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("farm");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Farm\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Grows grain in cycles\nBuilders reap it for the food that recruits civilians")).arg(productionPanel.format_cost_summary(0, builderFarmCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderFarmCard.construction_info.build_time || 10).toFixed(0)) : builderFarmCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderFarmMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderWallSegmentCard

                            property var construction_info: productionPanel.get_construction_info("wall_segment")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderWallSegmentMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderWallSegmentIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("wall_segment")
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderWallSegmentIcon.visible
                                text: Design.Icons.collect
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Wall Segment")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderWallSegmentCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: wallSegmentCostRow.implicitWidth + 8
                                        height: wallSegmentCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderWallSegmentCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderWallSegmentCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: wallSegmentCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderWallSegmentCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderWallSegmentMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("wall_segment");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Wall Segment\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Wooden defensive wall\nBlocks enemy movement")).arg(productionPanel.format_cost_summary(0, builderWallSegmentCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderWallSegmentCard.construction_info.build_time || 8).toFixed(0)) : builderWallSegmentCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderWallSegmentMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderWallGateCard

                            property var construction_info: productionPanel.get_construction_info("wall_gate")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderWallGateMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderWallGateIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("wall_gate")
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderWallGateIcon.visible
                                text: Design.Icons.gate
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Wall Gate")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderWallGateCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: wallGateCostRow.implicitWidth + 8
                                        height: wallGateCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderWallGateCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderWallGateCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: wallGateCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderWallGateCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderWallGateMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("wall_gate");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Wall Gate\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Gated opening in a wall\nOpens for your troops and allies")).arg(productionPanel.format_cost_summary(0, builderWallGateCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderWallGateCard.construction_info.build_time || 12).toFixed(0)) : builderWallGateCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderWallGateMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderMarketplaceCard

                            property var construction_info: productionPanel.get_construction_info("marketplace")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderMarketplaceMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderMarketplaceIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                source: productionPanel.unit_icon_source("marketplace")
                                visible: status === Image.Ready
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: builderMarketplaceIcon.status !== Image.Ready
                                text: Design.Icons.unitGlyph("marketplace")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Marketplace")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderMarketplaceCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: marketplaceCostRow.implicitWidth + 8
                                        height: marketplaceCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderMarketplaceCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderMarketplaceCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: marketplaceCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderMarketplaceCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderMarketplaceMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("marketplace");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Marketplace\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Trade building\nBuy or sell resources for gold")).arg(productionPanel.format_cost_summary(0, builderMarketplaceCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderMarketplaceCard.construction_info.build_time || 10).toFixed(0)) : builderMarketplaceCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderMarketplaceMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }

                        Rectangle {
                            id: builderTempleCard

                            property var construction_info: productionPanel.get_construction_info("temple")
                            property var card_state: productionPanel.construction_card_state(builderProductionContent.builder_prod, construction_info)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderTempleMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderTempleIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                source: productionPanel.unit_icon_source("temple")
                                visible: status === Image.Ready
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: builderTempleIcon.status !== Image.Ready
                                text: Design.Icons.unitGlyph("temple")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pixelSize: Design.Typography.glyph
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Temple")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(0, builderTempleCard.construction_info.resource_costs || {}, false)

                                    delegate: Rectangle {
                                        width: templeCostRow.implicitWidth + 8
                                        height: templeCostRow.implicitHeight + 6
                                        radius: 8
                                        color: builderTempleCard.is_enabled ? "#cc2a1d12" : "#991f150d"
                                        border.color: builderTempleCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: templeCostRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: Design.A11y.scaled(9)
                                                height: Design.A11y.scaled(9)
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderTempleCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pixelSize: Design.Typography.caption
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderTempleMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled) {
                                        Design.UiSound.activate();
                                        productionPanel.builder_construction("temple");
                                    } else {
                                        Design.UiSound.warning();
                                    }
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && parent.is_enabled)
                                        Design.UiSound.hover();
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Build Temple\n%1\nCost: %2\nBuild time: %3s").arg(qsTr("Sanctuary of the nation\nWide vision and a durable settlement anchor")).arg(productionPanel.format_cost_summary(0, builderTempleCard.construction_info.resource_costs || {}, qsTr("population"))).arg((builderTempleCard.construction_info.build_time || 10).toFixed(0)) : builderTempleCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderTempleMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: 150
                                }
                            }

                            Behavior on scale  {
                                NumberAnimation {
                                    duration: 100
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                property bool has_marketplace_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("marketplace")))

                width: parent.width
                height: marketplaceContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_marketplace_selected

                Column {
                    id: marketplaceContent

                    property var market_state: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_marketplace_state) ? productionPanel.production.selected_marketplace_state() : productionPanel.default_marketplace_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Image {
                            id: marketplaceHeaderIcon

                            width: 18
                            height: 18
                            source: productionPanel.unit_icon_source("marketplace", marketplaceContent.market_state.nation_id)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: status === Image.Ready
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: marketplaceHeaderIcon.visible ? qsTr("MARKETPLACE") : Design.Icons.unitGlyph("marketplace") + " " + qsTr("MARKETPLACE")
                            color: hs.bronze
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: marketplaceContent.market_state.has_marketplace ? qsTr("Trade resources for gold at fixed exchange rates") : qsTr("Select your marketplace to trade")
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: marketplaceContent.market_state.has_marketplace
                        text: qsTr("Gold: %1    Trade size: %2").arg(productionPanel.resource_amount(productionPanel.current_resources(), "gold")).arg(Math.max(0, marketplaceContent.market_state.trade_quantity || 0))
                        color: "#F4E7C8"
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: !marketplaceContent.market_state.has_marketplace
                        text: qsTr("Trading is available only for your own marketplace.")
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width - 24
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        visible: marketplaceContent.market_state.has_marketplace

                        Repeater {
                            model: productionPanel.marketplace_trade_specs

                            delegate: Rectangle {
                                property string resource_key: modelData.key
                                property int trade_quantity: Math.max(0, marketplaceContent.market_state.trade_quantity || 0)
                                property int buy_price: productionPanel.trade_price(marketplaceContent.market_state.buy_prices, resource_key)
                                property int sell_price: productionPanel.trade_price(marketplaceContent.market_state.sell_prices, resource_key)

                                width: marketplaceContent.width
                                height: 54
                                radius: 6
                                color: "#1A120C"
                                border.color: hs.bronzeDeep
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Image {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        source: productionPanel.cost_icon_source(resource_key)
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            text: modelData.label
                                            color: "#F4E7C8"
                                            font.pixelSize: Design.Typography.caption
                                            font.bold: true
                                        }

                                        Text {
                                            text: qsTr("You have %1").arg(productionPanel.resource_amount(productionPanel.current_resources(), resource_key))
                                            color: "#8D7146"
                                            font.pixelSize: Design.Typography.caption
                                        }
                                    }

                                    Button {

                                        readonly property bool allowed: productionPanel.can_buy_trade_resource(marketplaceContent.market_state, resource_key)

                                        Layout.preferredWidth: 110
                                        text: qsTr("Buy %1 (%2g)").arg(trade_quantity).arg(buy_price)
                                        opacity: allowed ? 1 : 0.5
                                        onClicked: {
                                            if (!allowed) {
                                                Design.UiSound.warning();
                                                return;
                                            }
                                            Design.UiSound.activate();
                                            productionPanel.production.marketplace_buy(resource_key);
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Spend %1 gold to buy %2 %3").arg(buy_price).arg(trade_quantity).arg(modelData.label.toLowerCase())
                                    }

                                    Button {

                                        readonly property bool allowed: productionPanel.can_sell_trade_resource(marketplaceContent.market_state, resource_key)

                                        Layout.preferredWidth: 110
                                        text: qsTr("Sell %1 (+%2g)").arg(trade_quantity).arg(sell_price)
                                        opacity: allowed ? 1 : 0.5
                                        onClicked: {
                                            if (!allowed) {
                                                Design.UiSound.warning();
                                                return;
                                            }
                                            Design.UiSound.activate();
                                            productionPanel.production.marketplace_sell(resource_key);
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Sell %1 %2 for %3 gold").arg(trade_quantity).arg(modelData.label.toLowerCase()).arg(sell_price)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                property bool has_temple_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("temple")))

                width: parent.width
                height: templeContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_temple_selected

                Column {
                    id: templeContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_temple_state) ? productionPanel.production.selected_temple_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Image {
                            id: templeHeaderIcon

                            width: 18
                            height: 18
                            source: productionPanel.unit_icon_source("temple")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: status === Image.Ready
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: templeHeaderIcon.visible ? qsTr("TEMPLE") : Design.Icons.unitGlyph("temple") + " " + qsTr("TEMPLE")
                            color: hs.bronze
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("The sanctuary of your nation, raised in its own architectural style")
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Watches over a wide stretch of ground and holds a settlement together")
                        color: "#F4E7C8"
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#3B2F24"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("TAKE VOWS")
                        color: hs.bronze
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: Math.max(Design.A11y.scaled(20), Design.Typography.label + 6)
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#120D09"
                        border.color: "#2F251D"
                        border.width: 2
                        visible: templeContent.prod.in_progress

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: {
                                if (!templeContent.prod.in_progress || templeContent.prod.build_time <= 0)
                                    return 0;
                                var progress = 1 - (Math.max(0, templeContent.prod.time_remaining) / templeContent.prod.build_time);
                                return Math.max(0, (parent.width - 4) * progress);
                            }
                            color: "#7F9A5F"
                            radius: 8
                        }

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("%1s").arg(Math.max(0, templeContent.prod.time_remaining).toFixed(1))
                            color: "#F4E7C8"
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Repeater {
                            model: productionPanel.temple_recruit_cards

                            delegate: RecruitCard {
                                required property var modelData

                                panel: productionPanel
                                prod: templeContent.prod
                                unit_type: modelData.unit_type
                                fallback_build_time: modelData.build_time
                                tooltip_text: panel ? panel.recruit_tooltip(unit_info, modelData.fallback_name, modelData.build_time, false) : ""
                                onRecruit_requested: function (unitType) {
                                    productionPanel.recruit_unit(unitType);
                                }
                                onDetails_requested: function (unitType, nation) {
                                    productionPanel.unit_details_requested(unitType, nation);
                                }
                            }
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Available Population: %1 / %2").arg(templeContent.prod.manpower_available || 0).arg(templeContent.prod.max_units || 0)
                        color: (templeContent.prod.manpower_available <= 0) ? "#C0403B" : "#D4B57C"
                        font.pixelSize: Design.Typography.caption
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Deliver civilians here to raise the temple's available population")
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }
                }
            }

            Rectangle {
                property bool has_farm_selected: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("farm")))

                width: parent.width
                height: farmContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_farm_selected

                Column {
                    id: farmContent

                    property var farm_state: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.selected_farm_state) ? productionPanel.production.selected_farm_state() : ({
                                "has_farm": false,
                                "growth": 0,
                                "ripe": false,
                                "seconds_to_ripe": 0,
                                "cycle_seconds": 60,
                                "yield": 0,
                                "harvests": 0,
                                "claimed": false
                            }))

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Image {
                            id: farmHeaderIcon

                            width: 18
                            height: 18
                            source: productionPanel.unit_icon_source("farm")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: status === Image.Ready
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: farmHeaderIcon.visible ? qsTr("FARM") : Design.Icons.unitGlyph("farm") + " " + qsTr("FARM")
                            color: hs.bronze
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Grain ripens every %1s and a builder reaps %2 food from it").arg(Math.round(farmContent.farm_state.cycle_seconds || 0)).arg(farmContent.farm_state.yield || 0)
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: Math.max(Design.A11y.scaled(20), Design.Typography.label + 6)
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#120D09"
                        border.color: "#2F251D"
                        border.width: 2

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: Math.max(0, (parent.width - 4) * Math.min(1, Math.max(0, farmContent.farm_state.growth || 0)))
                            color: farmContent.farm_state.ripe ? "#D9A441" : "#7F9A5F"
                            radius: 8
                        }

                        Text {
                            anchors.centerIn: parent
                            text: farmContent.farm_state.ripe ? qsTr("Ripe") : qsTr("%1% grown \u00b7 %2s").arg(Math.round((farmContent.farm_state.growth || 0) * 100)).arg(Math.ceil(farmContent.farm_state.seconds_to_ripe || 0))
                            color: "#F4E7C8"
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: {
                            if (!farmContent.farm_state.has_farm)
                                return qsTr("Select your farm to see its crop.");
                            if (farmContent.farm_state.claimed)
                                return qsTr("A builder is on its way to harvest.");
                            if (farmContent.farm_state.ripe)
                                return qsTr("Send a builder with Collect, or leave Auto Gather running.");
                            return qsTr("Harvested %1 times so far.").arg(farmContent.farm_state.harvests || 0);
                        }
                        color: farmContent.farm_state.ripe ? "#D9A441" : "#F4E7C8"
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        width: parent.width
                    }
                }
            }

            Item {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("barracks")))
                property bool has_builder: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("builder")))
                property bool has_home: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("home")))
                property bool has_marketplace: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("marketplace")))
                property bool has_temple: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("temple")))
                property bool has_farm: (productionPanel.selection_tick, (productionPanel.production && productionPanel.production.has_selected_type && productionPanel.production.has_selected_type("farm")))

                visible: !has_barracks && !has_builder && !has_home && !has_marketplace && !has_temple && !has_farm
                width: parent.width
                height: 200

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Design.Icons.unitGlyph("defense_tower")
                        color: "#3B2F24"
                        font.pixelSize: Design.Typography.glyph
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("No Barracks Selected")
                        color: "#8D7146"
                        font.pixelSize: Design.Typography.label
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Select a barracks to recruit units")
                        color: Theme.textSubLite
                        font.pixelSize: Design.Typography.caption
                    }
                }
            }
        }
    }
}
