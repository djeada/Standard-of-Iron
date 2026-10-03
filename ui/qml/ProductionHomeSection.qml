import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    required property var panel
    readonly property var hs: StyleGuide.historical
    property bool has_home: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("home")))

    width: parent.width
    height: homeProductionContent.height + 16
    color: hs.parchmentLight
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_home

    Column {
        id: homeProductionContent

        property var prod: (panel.selection_tick, (panel.production && panel.production.selected_home_state) ? panel.production.selected_home_state() : panel.default_production_state())

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
                var info = panel.get_unit_production_info("civilian", homeProductionContent.prod.nation_id);
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
            property var unit_info: panel.get_unit_production_info("civilian", homeProductionContent.prod.nation_id)
            property int committed_total: (homeProductionContent.prod.produced_count || 0) + queue_total
            property bool has_capacity: committed_total < (homeProductionContent.prod.max_units || 0)
            property bool has_families: (homeProductionContent.prod.manpower_available || 0) >= panel.reserve_cost(unit_info)
            property var recruit_state: panel.recruit_card_state(homeProductionContent.prod, unit_info, queue_total)
            property bool is_enabled: homeProductionContent.prod.has_home === true && has_capacity && recruit_state.enabled === true
            property bool is_hovered: civilianMouseArea.containsMouse

            width: 110
            height: 80
            anchors.horizontalCenter: parent.horizontalCenter
            radius: 6
            color: panel.recruit_card_color(is_enabled, is_hovered)
            border.color: panel.recruit_card_border(is_enabled, is_hovered)
            border.width: is_hovered && is_enabled ? 2 : 1
            opacity: is_enabled ? 1 : 0.5
            scale: is_hovered && is_enabled ? 1.025 : 1

            Image {
                id: civilianRecruitIcon

                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                smooth: true
                source: panel.unit_icon_source("civilian", homeProductionContent.prod.nation_id)
                visible: source !== ""
                opacity: parent.is_enabled ? 1 : 0.35
            }

            Text {
                anchors.centerIn: parent
                visible: !civilianRecruitIcon.visible
                text: panel.unit_icon_emoji("civilian")
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
                    model: panel.cost_entries(panel.reserve_cost(civilianCard.unit_info), civilianCard.unit_info.resource_costs || {}, true)

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
                                source: panel.cost_icon_source(modelData.key)
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
                        panel.recruit_unit("civilian");
                    } else {
                        Design.UiSound.warning();
                    }
                }
                cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                ToolTip.visible: containsMouse
                ToolTip.text: parent.is_enabled ? qsTr("Recruit %1\nCost: %2\nBuild time: %3s\nUse Deliver mode, then click a friendly barracks to add to its reserve.").arg(parent.unit_info.display_name || "Civilian").arg(panel.format_cost_summary(panel.reserve_cost(parent.unit_info), parent.unit_info.resource_costs || {}, qsTr("families"))).arg((parent.unit_info.build_time || 5).toFixed(0)) : (!civilianCard.has_capacity ? qsTr("This home already committed its 3 civilians") : civilianCard.recruit_state.reason)
                ToolTip.delay: 300
            }
        }
    }
}
