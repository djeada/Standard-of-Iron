import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: builderSection

    required property var panel
    readonly property var hs: StyleGuide.historical
    readonly property var card_specs: [{
            "item_type": "catapult",
            "label": qsTr("Catapult"),
            "description": qsTr("Long-range siege weapon\nEffective against structures"),
            "tooltip": qsTr("Build Catapult\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 15,
            "fit_icon": false,
            "glyph": ""
        }, {
            "item_type": "ballista",
            "label": qsTr("Ballista"),
            "description": qsTr("Precision siege weapon\nEffective against units"),
            "tooltip": qsTr("Build Ballista\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 12,
            "fit_icon": false,
            "glyph": ""
        }, {
            "item_type": "ram",
            "label": qsTr("Battering Ram"),
            "short_label": qsTr("Ram"),
            "description": qsTr("Gate-breaking siege engine\nImmune to most arrows"),
            "tooltip": qsTr("Build Battering Ram\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 14,
            "fit_icon": false,
            "glyph": ""
        }, {
            "item_type": "siege_tower",
            "label": qsTr("Siege Tower"),
            "description": qsTr("Docks against an enemy wall\nInfantry beside it climb up and cross"),
            "tooltip": qsTr("Build Siege Tower\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 22,
            "fit_icon": false,
            "glyph": ""
        }, {
            "item_type": "defense_tower",
            "label": qsTr("Defense Tower"),
            "short_label": qsTr("Tower"),
            "description": qsTr("Stationary defense structure\nShoots arrows at enemies"),
            "tooltip": qsTr("Build Defense Tower\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 20,
            "fit_icon": false,
            "glyph": Design.Icons.unitGlyph("defense_tower")
        }, {
            "item_type": "home",
            "label": qsTr("Home"),
            "description": qsTr("Residential building\nAdds +50 reserve to the nearest barracks"),
            "tooltip": qsTr("Build Home\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 10,
            "fit_icon": false,
            "glyph": Design.Icons.unitGlyph("home")
        }, {
            "item_type": "farm",
            "label": qsTr("Farm"),
            "description": qsTr("Grows grain in cycles\nBuilders reap it for the food that recruits civilians"),
            "tooltip": qsTr("Build Farm\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 10,
            "fit_icon": true,
            "glyph": Design.Icons.unitGlyph("farm")
        }, {
            "item_type": "wall_segment",
            "label": qsTr("Wall Segment"),
            "short_label": qsTr("Wall"),
            "description": qsTr("Wooden defensive wall\nBlocks enemy movement"),
            "tooltip": qsTr("Build Wall Segment\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 8,
            "fit_icon": false,
            "glyph": Design.Icons.collect
        }, {
            "item_type": "wall_gate",
            "label": qsTr("Wall Gate"),
            "short_label": qsTr("Gate"),
            "description": qsTr("Gated opening in a wall\nOpens for your troops and allies"),
            "tooltip": qsTr("Build Wall Gate\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 12,
            "fit_icon": false,
            "glyph": Design.Icons.gate
        }, {
            "item_type": "wall_ladder",
            "label": qsTr("Ladder"),
            "description": qsTr("Leans on the town side of your wall\nAnother way up onto the wall walk"),
            "tooltip": qsTr("Build Ladder\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 6,
            "fit_icon": false,
            "glyph": Design.Icons.unitGlyph("wall_ladder")
        }, {
            "item_type": "marketplace",
            "label": qsTr("Marketplace"),
            "short_label": qsTr("Market"),
            "description": qsTr("Trade building\nBuy or sell resources for gold"),
            "tooltip": qsTr("Build Marketplace\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 10,
            "fit_icon": true,
            "glyph": Design.Icons.unitGlyph("marketplace")
        }, {
            "item_type": "temple",
            "label": qsTr("Temple"),
            "description": qsTr("Sanctuary of the nation\nWide vision and a durable settlement anchor"),
            "tooltip": qsTr("Build Temple\n%1\nCost: %2\nBuild time: %3s"),
            "default_build_time": 10,
            "fit_icon": true,
            "glyph": Design.Icons.unitGlyph("temple")
        }]
    property bool has_builder: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("builder")))

    width: parent.width
    height: builderProductionContent.height + 16
    color: "#120D09"
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_builder

    Column {
        id: builderProductionContent

        property var builder_prod: (panel.selection_tick, (panel.production && panel.production.selected_builder_state) ? panel.production.selected_builder_state() : {
                "in_progress": false,
                "build_time": 10,
                "time_remaining": 0,
                "product_type": ""
            })

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 5
        width: parent.width - 12

        Rectangle {
            width: parent.width - 20
            height: Math.max(Design.A11y.scaled(16), Design.Typography.caption + 4)
            anchors.horizontalCenter: parent.horizontalCenter
            radius: 10
            color: "#120D09"
            border.color: "#2F251D"
            border.width: 2
            visible: builderProductionContent.builder_prod.in_progress === true

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
                text: builderProductionContent.builder_prod.in_progress ? qsTr("%1 · %2s").arg(builderStatus.text).arg(Math.max(0, builderProductionContent.builder_prod.time_remaining).toFixed(1)) : qsTr("Idle")
                color: "#F4E7C8"
                font.pixelSize: Design.Typography.caption
                font.bold: true
                style: Text.Outline
                styleColor: "#120D09"
            }
        }

        // Names what is being built for the progress bar; the selection panel
        // already says these are builders, so the cards need no header.
        Text {
            id: builderStatus

            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 1
            elide: Text.ElideRight
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
                } else if (label === "wall_ladder") {
                    label = qsTr("Ladder");
                }
                return (is_collection_task ? qsTr("Task: %1") : qsTr("Building: %1")).arg(label);
            }
            color: "#8D7146"
            font.pixelSize: Design.Typography.caption
            visible: false
        }

        Grid {
            id: builderCardGrid

            // Every order a builder has fits in two rows of the bottom HUD, as on a
            // castle builder's command bar: no scrolling to find the ram.
            readonly property int minCardWidth: 74
            readonly property int cardWidth: Math.floor((parent.width - (columns - 1) * columnSpacing) / columns)
            readonly property int cardHeight: 64

            objectName: "builderCardGrid"

            anchors.horizontalCenter: parent.horizontalCenter

            columns: Math.max(1, Math.min(6, Math.floor((parent.width + columnSpacing) / (minCardWidth + columnSpacing))))
            columnSpacing: 5
            rowSpacing: 5

            Repeater {
                model: builderSection.card_specs

                delegate: ProductionConstructionCard {
                    required property var modelData

                    panel: builderSection.panel
                    builder_prod: builderProductionContent.builder_prod
                    spec: modelData
                    cardWidth: builderCardGrid.cardWidth
                    cardHeight: builderCardGrid.cardHeight
                }
            }
        }
    }
}
