import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    required property var panel
    readonly property var hs: StyleGuide.historical
    property bool has_barracks: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("barracks")))

    width: parent.width
    height: unitGridContent.height + 12
    color: hs.parchmentLight
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_barracks

    Column {
        id: unitGridContent

        property var prod: (panel.selection_tick, (panel.production && panel.production.selected_state) ? panel.production.selected_state() : panel.default_production_state())

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 6
        width: parent.width - 12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("RECRUIT UNITS")
            color: hs.bronze
            font.pixelSize: Design.Typography.caption
            font.bold: true
        }

        Grid {
            id: recruitGrid

            readonly property int cardSpacing: 6
            readonly property int minCardWidth: Design.A11y.scaled(120)
            readonly property int cardWidth: Math.floor((width - recruitGrid.cardSpacing * (recruitGrid.columns - 1)) / recruitGrid.columns)

            width: parent.width
            columns: Math.max(2, Math.floor((width + recruitGrid.cardSpacing) / (recruitGrid.minCardWidth + recruitGrid.cardSpacing)))
            columnSpacing: recruitGrid.cardSpacing
            rowSpacing: recruitGrid.cardSpacing

            Repeater {
                model: panel.recruit_unit_cards

                delegate: RecruitCard {
                    required property var modelData

                    width: recruitGrid.cardWidth
                    height: Design.A11y.scaled(58)

                    panel: productionPanel
                    prod: unitGridContent.prod
                    unit_type: modelData.unit_type
                    fallback_name: modelData.fallback_name
                    fallback_build_time: modelData.build_time
                    tooltip_text: panel ? panel.recruit_tooltip(unit_info, modelData.fallback_name, modelData.build_time, modelData.carthage_only === true) : ""
                    visible: modelData.carthage_only !== true || unitGridContent.prod.nation_id === "carthage"
                    onRecruit_requested: function (unitType) {
                        panel.recruit_unit(unitType);
                    }
                    onDetails_requested: function (unitType, nation) {
                        panel.unit_details_requested(unitType, nation);
                    }
                }
            }
        }
    }
}
