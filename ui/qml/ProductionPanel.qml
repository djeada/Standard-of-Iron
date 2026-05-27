import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: productionPanel

    property int selection_tick: 0
    property var game_instance: null
    readonly property var hs: StyleGuide.historical

    signal recruit_unit(string unit_type)
    signal rally_mode_toggled
    signal build_tower
    signal builder_construction(string item_type)

    readonly property var recruit_unit_cards: ["archer", "swordsman", "spearman", "horse_swordsman", "horse_archer", "horse_spearman", "healer", "builder", "elephant"]
    readonly property var marketplace_trade_specs: [{
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
            "fallback_emoji": "🏰"
        }, {
            "item_type": "home",
            "label": qsTr("Home"),
            "description": qsTr("Residential building\nAdds +50 population to nearest barracks"),
            "fallback_emoji": "🏠"
        }, {
            "item_type": "marketplace",
            "label": qsTr("Marketplace"),
            "description": qsTr("Trade building\nBuy or sell resources for gold"),
            "fallback_emoji": "🏪"
        }, {
            "item_type": "wall_segment",
            "label": qsTr("Wall Segment"),
            "description": qsTr("Wooden defensive wall\nBlocks enemy movement"),
            "fallback_emoji": "🪵"
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
            "has_home": false
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
        if (typeof StyleGuide === "undefined" || !StyleGuide.unit_icon_sources || !unit_type)
            return "";
        var sources = StyleGuide.unit_icon_sources[unit_type];
        if (!sources)
            sources = StyleGuide.unit_icon_sources["default"];
        if (typeof sources === "object" && sources !== null) {
            if (nation_key && sources[nation_key])
                return sources[nation_key];
            if (sources["default"])
                return sources["default"];
        } else if (typeof sources === "string") {
            return sources;
        }
        return "";
    }

    function unit_icon_emoji(unit_type) {
        if (unit_type && unit_type.indexOf("commander") !== -1 || unit_type === "roman_legion_organizer" || unit_type === "roman_veteran_consul" || unit_type === "carthage_mercenary_broker" || unit_type === "carthage_cavalry_patron" || unit_type === "carthage_elephant_master")
            return "⚜";
        if (typeof StyleGuide !== "undefined" && StyleGuide.unit_icons)
            return StyleGuide.unit_icons[unit_type] || StyleGuide.unit_icons["default"] || "👤";
        return "👤";
    }

    function get_unit_production_info(unit_type, nation_id) {
        if (productionPanel.game_instance && productionPanel.game_instance.get_unit_production_info)
            return productionPanel.game_instance.get_unit_production_info(unit_type, nation_id || "");
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
        if (productionPanel.game_instance && productionPanel.game_instance.get_construction_info)
            return productionPanel.game_instance.get_construction_info(item_type || "");
        return {
            "build_time": 10,
            "resource_costs": {},
            "display_name": item_type
        };
    }

    function current_resources() {
        if (productionPanel.game_instance && productionPanel.game_instance.selected_player_state && productionPanel.game_instance.selected_player_state.resources)
            return productionPanel.game_instance.selected_player_state.resources;
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

    function missing_resource_names(costs) {
        var resources = current_resources();
        var missing = [];
        var ordered = [{
                "key": "wood",
                "label": qsTr("wood")
            }, {
                "key": "stone",
                "label": qsTr("stone")
            }, {
                "key": "iron",
                "label": qsTr("iron")
            }, {
                "key": "gold",
                "label": qsTr("gold")
            }];
        for (var i = 0; i < ordered.length; ++i) {
            var entry = ordered[i];
            if (resource_amount(resources, entry.key) < resource_amount(costs, entry.key))
                missing.push(entry.label);
        }
        return missing;
    }

    function can_afford_resource_costs(costs) {
        return missing_resource_names(costs).length === 0;
    }

    function cost_entries(popCost, resourceCosts, includePopulation) {
        var entries = [];
        if (includePopulation && popCost > 0)
            entries.push({
                    "key": "population",
                    "amount": popCost
                });
        var ordered = ["wood", "stone", "iron", "gold"];
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
        var woodCost = resource_amount(resourceCosts, "wood");
        var stoneCost = resource_amount(resourceCosts, "stone");
        var ironCost = resource_amount(resourceCosts, "iron");
        var goldCost = resource_amount(resourceCosts, "gold");
        if (woodCost > 0)
            parts.push(qsTr("%1 wood").arg(woodCost));
        if (stoneCost > 0)
            parts.push(qsTr("%1 stone").arg(stoneCost));
        if (ironCost > 0)
            parts.push(qsTr("%1 iron").arg(ironCost));
        if (goldCost > 0)
            parts.push(qsTr("%1 gold").arg(goldCost));
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
        if (!(prod.has_barracks || prod.has_home))
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
        var missing = missing_resource_names(unitInfo.resource_costs || {});
        if (missing.length > 0)
            return {
                "enabled": false,
                "reason": qsTr("Not enough %1").arg(missing.join(", "))
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
        var missing = missing_resource_names(constructionInfo.resource_costs || {});
        if (missing.length > 0)
            return {
                "enabled": false,
                "reason": qsTr("Not enough %1").arg(missing.join(", "))
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
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("barracks")))

                width: parent.width
                height: productionContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: productionContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.get_selected_production_state) ? productionPanel.game_instance.get_selected_production_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 10
                    width: parent.width - 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("PRODUCTION QUEUE")
                        color: hs.bronze
                        font.pointSize: 8
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

                                width: 36
                                height: 36
                                radius: 6
                                color: is_producing ? "#7F9A5F" : (is_occupied ? "#2F251D" : "#120D09")
                                border.color: is_producing ? "#8FA46B" : (is_occupied ? "#6F8E8C" : "#3B2F24")
                                border.width: 2

                                Image {
                                    id: queueIconImage

                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    source: parent.is_occupied ? productionPanel.unit_icon_source(parent.queue_unit_type, productionContent.prod.nation_id) : ""
                                    visible: parent.is_occupied && source !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.is_occupied ? productionPanel.unit_icon_emoji(parent.queue_unit_type) : "·"
                                    color: parent.is_producing ? "#F4E7C8" : (parent.is_occupied ? "#D4B57C" : "#6B5231")
                                    font.pointSize: parent.is_occupied ? 16 : 20
                                    font.bold: parent.is_producing
                                    visible: !queueIconImage.visible
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    text: (index + 1).toString()
                                    color: parent.is_occupied ? "#8D7146" : "#3B2F24"
                                    font.pointSize: 7
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
                        font.pointSize: 9
                        font.bold: queue_total >= 5
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: 20
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
                            text: productionContent.prod.in_progress ? Math.max(0, productionContent.prod.time_remaining).toFixed(1) + "s" : "Idle"
                            color: "#F4E7C8"
                            font.pointSize: 9
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Available Population: %1 / %2").arg(productionContent.prod.manpower_available || 0).arg(productionContent.prod.max_units || 0)
                        color: (productionContent.prod.manpower_available <= 0) ? "#C0403B" : "#D4B57C"
                        font.pointSize: 8
                    }
                }
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("barracks")))

                width: parent.width
                height: unitGridContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: unitGridContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.get_selected_production_state) ? productionPanel.game_instance.get_selected_production_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("RECRUIT UNITS")
                        color: hs.bronze
                        font.pointSize: 8
                        font.bold: true
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Rectangle {
                            id: archerCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("archer", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: archerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: archerRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("archer", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !archerRecruitIcon.visible
                                text: productionPanel.unit_icon_emoji("archer")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(archerCard.unit_info), archerCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: archerCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: archerCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: archerCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: archerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("archer");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Archer").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 5).toFixed(0)) : archerCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: archerMouseArea.pressed ? 0.2 : 0
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
                            id: swordsmanCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("swordsman", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: swordsmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: swordsmanRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("swordsman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !swordsmanRecruitIcon.visible
                                text: productionPanel.unit_icon_emoji("swordsman")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(swordsmanCard.unit_info), swordsmanCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: swordsmanCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: swordsmanCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: swordsmanCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: swordsmanMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("swordsman");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Swordsman").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 7).toFixed(0)) : swordsmanCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: swordsmanMouseArea.pressed ? 0.2 : 0
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
                            id: spearmanCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("spearman", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: spearmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: spearmanRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("spearman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !spearmanRecruitIcon.visible
                                text: productionPanel.unit_icon_emoji("spearman")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(spearmanCard.unit_info), spearmanCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: spearmanCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: spearmanCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: spearmanCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: spearmanMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("spearman");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Spearman").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 6).toFixed(0)) : spearmanCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: spearmanMouseArea.pressed ? 0.2 : 0
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
                            id: horseKnightCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("horse_swordsman", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: horseKnightMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: horseKnightIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("horse_swordsman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !horseKnightIcon.visible
                                text: productionPanel.unit_icon_emoji("horse_swordsman")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(horseKnightCard.unit_info), horseKnightCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: horseKnightCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: horseKnightCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: horseKnightCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: horseKnightMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("horse_swordsman");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Mounted Knight").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 10).toFixed(0)) : horseKnightCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: horseKnightMouseArea.pressed ? 0.2 : 0
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
                            id: horseArcherCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("horse_archer", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: horseArcherMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: horseArcherIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("horse_archer", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !horseArcherIcon.visible
                                text: productionPanel.unit_icon_emoji("horse_archer")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(horseArcherCard.unit_info), horseArcherCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: horseArcherCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: horseArcherCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: horseArcherCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: horseArcherMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("horse_archer");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Horse Archer").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 9).toFixed(0)) : horseArcherCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: horseArcherMouseArea.pressed ? 0.2 : 0
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
                            id: horseSpearmanCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("horse_spearman", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: horseSpearmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: horseSpearmanIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("horse_spearman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !horseSpearmanIcon.visible
                                text: productionPanel.unit_icon_emoji("horse_spearman")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(horseSpearmanCard.unit_info), horseSpearmanCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: horseSpearmanCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: horseSpearmanCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: horseSpearmanCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: horseSpearmanMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("horse_spearman");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Horse Spearman").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 9).toFixed(0)) : horseSpearmanCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: horseSpearmanMouseArea.pressed ? 0.2 : 0
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
                            id: healerCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("healer", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: healerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: healerRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("healer", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !healerRecruitIcon.visible
                                text: productionPanel.unit_icon_emoji("healer")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(healerCard.unit_info), healerCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: healerCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: healerCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: healerCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: healerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("healer");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Healer").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 8).toFixed(0)) : healerCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: healerMouseArea.pressed ? 0.2 : 0
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
                            id: builderRecruitCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("builder", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: builderMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1

                            Image {
                                id: builderRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("builder", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderRecruitIcon.visible
                                text: productionPanel.unit_icon_emoji("builder")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(builderRecruitCard.unit_info), builderRecruitCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderRecruitCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderRecruitCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderRecruitCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: builderMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("builder");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s").arg(parent.unit_info.display_name || "Builder").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 6).toFixed(0)) : builderRecruitCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderMouseArea.pressed ? 0.2 : 0
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
                            id: elephantCard

                            property int queue_total: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property var unit_info: productionPanel.get_unit_production_info("elephant", unitGridContent.prod.nation_id)
                            property var card_state: productionPanel.recruit_card_state(unitGridContent.prod, unit_info, queue_total)
                            property bool is_enabled: card_state.enabled
                            property bool is_hovered: elephantMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruit_card_color(is_enabled, is_hovered)
                            border.color: productionPanel.recruit_card_border(is_enabled, is_hovered)
                            border.width: is_hovered && is_enabled ? 2 : 1
                            opacity: is_enabled ? 1 : 0.5
                            scale: is_hovered && is_enabled ? 1.025 : 1
                            visible: unitGridContent.prod.nation_id === "carthage"

                            Image {
                                id: elephantRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unit_icon_source("elephant", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.is_enabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !elephantRecruitIcon.visible
                                text: productionPanel.unit_icon_emoji("elephant")
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Flow {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                spacing: 4

                                Repeater {
                                    model: productionPanel.cost_entries(productionPanel.population_cost(elephantCard.unit_info), elephantCard.unit_info.resource_costs || {}, true)

                                    delegate: Rectangle {
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: elephantCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: elephantCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: elephantCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: elephantMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.is_enabled)
                                        productionPanel.recruit_unit("elephant");
                                }
                                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s\nCarthage exclusive").arg(parent.unit_info.display_name || "War Elephant").arg(productionPanel.format_cost_summary(productionPanel.population_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("population"))).arg((parent.unit_info.build_time || 20).toFixed(0)) : elephantCard.card_state.reason
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: elephantMouseArea.pressed ? 0.2 : 0
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
                property bool has_home: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("home")))

                width: parent.width
                height: homeProductionContent.height + 16
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
                border.width: 1
                visible: has_home

                Column {
                    id: homeProductionContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.get_selected_home_production_state) ? productionPanel.game_instance.get_selected_home_production_state() : productionPanel.default_production_state())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("HOME RECRUITMENT")
                        color: "#3498db"
                        font.pointSize: 8
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
                        color: "#bdc3c7"
                        font.pointSize: 8
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
                        color: is_enabled ? (is_hovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                        border.color: is_enabled ? (is_hovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                        border.width: is_hovered && is_enabled ? 4 : 2
                        opacity: is_enabled ? 1 : 0.5
                        scale: is_hovered && is_enabled ? 1.08 : 1

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
                            color: parent.is_enabled ? "#ecf0f1" : "#5a5a5a"
                            font.pointSize: 36
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
                                    width: costRow.implicitWidth + 8
                                    height: 16
                                    radius: 8
                                    color: civilianCard.is_enabled ? "#000000b3" : "#00000066"
                                    border.color: civilianCard.is_enabled ? "#f39c12" : "#555555"
                                    border.width: 1

                                    Row {
                                        id: costRow

                                        anchors.centerIn: parent
                                        spacing: 3

                                        Image {
                                            width: 9
                                            height: 9
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            source: productionPanel.cost_icon_source(modelData.key)
                                        }

                                        Text {
                                            text: modelData.amount
                                            color: civilianCard.is_enabled ? "#fdf7e3" : "#8a8a8a"
                                            font.pointSize: 7
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
                                if (parent.is_enabled)
                                    productionPanel.recruit_unit("civilian");
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
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("barracks")))

                width: parent.width
                height: 1
                color: "#3B2F24"
                visible: has_barracks || (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("home"))
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("barracks")))

                width: parent.width
                height: rallyContent.height + 12
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: rallyContent

                    property var prod: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.get_selected_production_state) ? productionPanel.game_instance.get_selected_production_state() : productionPanel.default_production_state())
                    property bool placing_barracks_rally: typeof gameView !== 'undefined' && gameView.cursor_mode === "place_barracks_rally"

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 6

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.parent.width - 20
                        height: 32
                        text: rallyContent.placing_barracks_rally ? qsTr("📍 Click Map to Set Rally") : qsTr("📍 Set Rally Point")
                        focusPolicy: Qt.NoFocus
                        enabled: rallyContent.prod.has_barracks
                        onClicked: productionPanel.rally_mode_toggled()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Set where newly recruited units will gather.\nRight-click to cancel.")
                        ToolTip.delay: 500

                        background: Rectangle {
                            color: parent.enabled ? (parent.down ? hs.bronzeDeep : (parent.hovered ? hs.bronze : hs.parchmentDark)) : Theme.bgShade
                            radius: 6
                            border.color: rallyContent.placing_barracks_rally ? hs.bronze : hs.bronzeDeep
                            border.width: 2
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pointSize: 9
                            font.bold: true
                            color: parent.enabled ? "#F4E7C8" : "#6B5231"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: rallyContent.placing_barracks_rally ? qsTr("Right-click to cancel") : ""
                        color: "#8D7146"
                        font.pointSize: 8
                        font.italic: true
                    }
                }
            }

            Item {
                property bool has_barracks_selected: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("barracks")))
                property bool has_home_selected: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("home")))

                height: 20
                visible: !has_barracks_selected && !has_home_selected
            }

            Rectangle {
                property bool has_builder: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("builder")))

                width: parent.width
                height: builderProductionContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_builder

                Column {
                    id: builderProductionContent

                    property var builder_prod: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.get_selected_builder_production_state) ? productionPanel.game_instance.get_selected_builder_production_state() : {
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
                            text: builderHeaderIcon.visible ? qsTr("BUILDER CONSTRUCTION") : qsTr("🔨 BUILDER CONSTRUCTION")
                            color: hs.bronze
                            font.pointSize: 9
                            font.bold: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Build siege weapons, structures, and gather wood, stone, and iron")
                        color: "#8D7146"
                        font.pointSize: 7
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: 20
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
                            text: builderProductionContent.builder_prod.in_progress ? Math.max(0, builderProductionContent.builder_prod.time_remaining).toFixed(1) + "s" : "Idle"
                            color: "#F4E7C8"
                            font.pointSize: 9
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
                            } else if (label === "wall_segment") {
                                label = qsTr("Wall Segment");
                            }
                            return (is_collection_task ? qsTr("Task: %1") : qsTr("Building: %1")).arg(label);
                        }
                        color: builderProductionContent.builder_prod.in_progress ? "#7F9A5F" : "#8D7146"
                        font.pointSize: 8
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
                                font.pointSize: 36
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Catapult")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
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
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderCatapultCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderCatapultCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderCatapultCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
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
                                    if (parent.is_enabled)
                                        productionPanel.builder_construction("catapult");
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
                                font.pointSize: 36
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Ballista")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
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
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderBallistaCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderBallistaCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderBallistaCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
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
                                    if (parent.is_enabled)
                                        productionPanel.builder_construction("ballista");
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
                                text: "🏰"
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 36
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Defense Tower")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
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
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderDefenseTowerCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderDefenseTowerCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderDefenseTowerCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
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
                                    if (parent.is_enabled)
                                        productionPanel.builder_construction("defense_tower");
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
                                text: "🏠"
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 36
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Home")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
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
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderHomeCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderHomeCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderHomeCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
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
                                    if (parent.is_enabled)
                                        productionPanel.builder_construction("home");
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
                                text: "🪵"
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 34
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Wall Segment")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
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
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderWallSegmentCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderWallSegmentCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderWallSegmentCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
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
                                    if (parent.is_enabled)
                                        productionPanel.builder_construction("wall_segment");
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
                                text: "🏪"
                                color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 34
                                opacity: parent.is_enabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 24
                                text: qsTr("Marketplace")
                                color: parent.is_enabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
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
                                        width: costRow.implicitWidth + 8
                                        height: 16
                                        radius: 8
                                        color: builderMarketplaceCard.is_enabled ? "#2a1d12cc" : "#1f150d99"
                                        border.color: builderMarketplaceCard.is_enabled ? hs.bronze : "#8C6A3E"
                                        border.width: 1

                                        Row {
                                            id: costRow

                                            anchors.centerIn: parent
                                            spacing: 3

                                            Image {
                                                width: 9
                                                height: 9
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                source: productionPanel.cost_icon_source(modelData.key)
                                            }

                                            Text {
                                                text: modelData.amount
                                                color: builderMarketplaceCard.is_enabled ? Theme.textMain : Theme.textDim
                                                font.pointSize: 7
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
                                    if (parent.is_enabled)
                                        productionPanel.builder_construction("marketplace");
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
                    }
                }
            }

            Rectangle {
                property bool has_marketplace_selected: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("marketplace")))

                width: parent.width
                height: marketplaceContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_marketplace_selected

                Column {
                    id: marketplaceContent

                    property var market_state: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.get_selected_marketplace_state) ? productionPanel.game_instance.get_selected_marketplace_state() : productionPanel.default_marketplace_state())

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
                            text: marketplaceHeaderIcon.visible ? qsTr("MARKETPLACE") : qsTr("🏪 MARKETPLACE")
                            color: hs.bronze
                            font.pointSize: 9
                            font.bold: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: marketplaceContent.market_state.has_marketplace ? qsTr("Trade resources for gold at fixed exchange rates") : qsTr("Select your marketplace to trade")
                        color: "#8D7146"
                        font.pointSize: 7
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: marketplaceContent.market_state.has_marketplace
                        text: qsTr("Gold: %1    Trade size: %2").arg(productionPanel.resource_amount(productionPanel.current_resources(), "gold")).arg(Math.max(0, marketplaceContent.market_state.trade_quantity || 0))
                        color: "#F4E7C8"
                        font.pointSize: 8
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: !marketplaceContent.market_state.has_marketplace
                        text: qsTr("Trading is available only for your own marketplace.")
                        color: "#8D7146"
                        font.pointSize: 8
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
                                            font.pointSize: 8
                                            font.bold: true
                                        }

                                        Text {
                                            text: qsTr("You have %1").arg(productionPanel.resource_amount(productionPanel.current_resources(), resource_key))
                                            color: "#8D7146"
                                            font.pointSize: 7
                                        }
                                    }

                                    Button {
                                        Layout.preferredWidth: 110
                                        text: qsTr("Buy %1 (%2g)").arg(trade_quantity).arg(buy_price)
                                        enabled: productionPanel.can_buy_trade_resource(marketplaceContent.market_state, resource_key)
                                        onClicked: productionPanel.game_instance.marketplace_buy_resource(resource_key)
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Spend %1 gold to buy %2 %3").arg(buy_price).arg(trade_quantity).arg(modelData.label.toLowerCase())
                                    }

                                    Button {
                                        Layout.preferredWidth: 110
                                        text: qsTr("Sell %1 (+%2g)").arg(trade_quantity).arg(sell_price)
                                        enabled: productionPanel.can_sell_trade_resource(marketplaceContent.market_state, resource_key)
                                        onClicked: productionPanel.game_instance.marketplace_sell_resource(resource_key)
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Sell %1 %2 for %3 gold").arg(trade_quantity).arg(modelData.label.toLowerCase()).arg(sell_price)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                property bool has_barracks: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("barracks")))
                property bool has_builder: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("builder")))
                property bool has_home: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("home")))
                property bool has_marketplace: (productionPanel.selection_tick, (productionPanel.game_instance && productionPanel.game_instance.has_selected_type && productionPanel.game_instance.has_selected_type("marketplace")))

                visible: !has_barracks && !has_builder && !has_home && !has_marketplace
                width: parent.width
                height: 200

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🏰"
                        color: "#3B2F24"
                        font.pointSize: 32
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("No Barracks Selected")
                        color: "#8D7146"
                        font.pointSize: 11
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Select a barracks to recruit units")
                        color: Theme.textSubLite
                        font.pointSize: 9
                    }
                }
            }
        }
    }
}
