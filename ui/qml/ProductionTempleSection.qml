import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    required property var panel
    readonly property var hs: StyleGuide.historical
    property bool has_temple_selected: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("temple")))

    width: parent.width
    height: templeContent.height + 16
    color: "#120D09"
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_temple_selected

    Column {
        id: templeContent

        property var prod: (panel.selection_tick, (panel.production && panel.production.selected_temple_state) ? panel.production.selected_temple_state() : panel.default_production_state())

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
                source: panel.unit_icon_source("temple")
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
            visible: templeContent.prod.in_progress === true

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
                model: panel.temple_recruit_cards

                delegate: RecruitCard {
                    required property var modelData

                    panel: productionPanel
                    prod: templeContent.prod
                    unit_type: modelData.unit_type
                    fallback_name: modelData.fallback_name
                    fallback_build_time: modelData.build_time
                    tooltip_text: panel ? panel.recruit_tooltip(unit_info, modelData.fallback_name, modelData.build_time, false) : ""
                    onRecruit_requested: function (unitType) {
                        panel.recruit_unit(unitType);
                    }
                    onDetails_requested: function (unitType, nation) {
                        panel.unit_details_requested(unitType, nation);
                    }
                }
            }
        }

        Text {
            objectName: "templeReserveLabel"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Temple reserve: %1 / %2").arg(templeContent.prod.manpower_available || 0).arg(templeContent.prod.max_units || 0)
            color: (templeContent.prod.manpower_available <= 0) ? "#C0403B" : "#D4B57C"
            font.pixelSize: Design.Typography.caption
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Deliver civilians here to raise the temple's reserve")
            color: "#8D7146"
            font.pixelSize: Design.Typography.caption
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }
    }
}
