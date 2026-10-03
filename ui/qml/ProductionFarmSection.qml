import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    required property var panel
    readonly property var hs: StyleGuide.historical
    property bool has_farm_selected: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("farm")))

    width: parent.width
    height: farmContent.height + 16
    color: "#120D09"
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_farm_selected

    Column {
        id: farmContent

        property var farm_state: (panel.selection_tick, (panel.production && panel.production.selected_farm_state) ? panel.production.selected_farm_state() : ({
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
                source: panel.unit_icon_source("farm")
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
