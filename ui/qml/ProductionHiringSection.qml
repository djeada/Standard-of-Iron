import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: hiringSection

    required property var panel
    readonly property var hs: StyleGuide.historical
    readonly property bool has_hiring_selected: (panel.selection_tick, (panel.production && panel.production.has_selected_type && (panel.production.has_selected_type("mercenary_camp") || panel.production.has_selected_type("elephant_stables"))))
    readonly property var hiring: (panel.selection_tick, (hiringSection.has_hiring_selected && panel.production && panel.production.selected_hiring_state) ? panel.production.selected_hiring_state() : ({}))
    readonly property bool is_camp: hiringSection.hiring.building_type === "mercenary_camp"

    width: parent.width
    height: hiringContent.height + 16
    color: "#120D09"
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_hiring_selected && hiringSection.hiring.has_hiring === true

    Column {
        id: hiringContent

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 6
        width: parent.width - 16

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: hiringSection.is_camp ? qsTr("MERCENARY CAMP") : qsTr("ELEPHANT STABLES")
            color: hs.bronze
            font.pixelSize: Design.Typography.caption
            font.bold: true
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: hiringSection.is_camp ? qsTr("Hire a whole band for gold. No reserve, no iron - but every band is paid every minute.") : qsTr("Elephants for gold and food. No reserve; at most eight alive.")
            color: "#8D7146"
            font.pixelSize: Design.Typography.caption
        }

        Text {
            objectName: "hiringUpkeepLabel"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: hiringSection.is_camp
            text: qsTr("Upkeep %1 gold a minute - trade brings %2").arg(hiringSection.hiring.upkeep_per_minute || 0).arg(hiringSection.hiring.income_per_minute || 0)
            color: (hiringSection.hiring.upkeep_per_minute || 0) > (hiringSection.hiring.income_per_minute || 0) ? "#C0403B" : "#D4B57C"
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
            visible: hiringSection.hiring.in_progress === true

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 2
                height: parent.height - 4
                width: {
                    var total = hiringSection.hiring.build_time || 0;
                    if (!hiringSection.hiring.in_progress || total <= 0)
                        return 0;
                    var progress = 1 - (Math.max(0, hiringSection.hiring.time_remaining || 0) / total);
                    return Math.max(0, (parent.width - 4) * progress);
                }
                color: "#7F9A5F"
                radius: 8
            }

            Text {
                anchors.centerIn: parent
                text: qsTr("Contract signed - %1s").arg(Math.max(0, hiringSection.hiring.time_remaining || 0).toFixed(1))
                color: "#F4E7C8"
                font.pixelSize: Design.Typography.caption
                font.bold: true
                style: Text.Outline
                styleColor: "#120D09"
            }
        }

        Repeater {
            model: hiringSection.hiring.offers || []

            delegate: Rectangle {
                id: offerCard

                required property var modelData
                readonly property int gold: (modelData.resource_costs && modelData.resource_costs.gold) || 0
                readonly property int food: (modelData.resource_costs && modelData.resource_costs.food) || 0
                readonly property bool blocked: modelData.affordable !== true || hiringSection.hiring.in_progress === true

                objectName: "hiringOffer_" + modelData.key
                width: hiringContent.width
                height: Math.max(Design.A11y.scaled(36), offerText.implicitHeight + 10)
                radius: 4
                color: offerMouse.containsMouse && !offerCard.blocked ? Qt.lighter(hs.parchmentLight, 1.3) : (offerCard.blocked ? Design.Theme.bgShade : hs.parchmentLight)
                border.color: offerCard.blocked ? hs.parchmentLight : hs.bronzeDeep
                border.width: 1

                Column {
                    id: offerText

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 6
                    spacing: 1

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: offerCard.modelData.display_name
                        color: "#2A1E14"
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: offerCard.food > 0 ? qsTr("%1 men - %2 gold, %3 food").arg(offerCard.modelData.men).arg(offerCard.gold).arg(offerCard.food) : qsTr("%1 men - %2 gold - upkeep %3 a minute").arg(offerCard.modelData.men).arg(offerCard.gold).arg(offerCard.modelData.upkeep_per_minute)
                        color: "#4A3A28"
                        font.pixelSize: Design.Typography.caption
                    }
                }

                MouseArea {
                    id: offerMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: hiringSection.panel.recruit_unit(offerCard.modelData.key)
                }
            }
        }
    }
}
