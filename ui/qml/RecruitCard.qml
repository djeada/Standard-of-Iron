import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    id: card

    property var panel: null
    property var prod: ({})
    property string unit_type: ""
    property string fallback_name: ""
    property int fallback_build_time: 5
    property string tooltip_text: ""

    readonly property var hs: StyleGuide.historical

    readonly property int queue_total: (card.prod.in_progress ? 1 : 0) + (card.prod.queue_size || 0)
    readonly property var unit_info: card.panel ? card.panel.get_unit_production_info(card.unit_type, card.prod.nation_id) : ({})
    readonly property var card_state: card.panel ? card.panel.recruit_card_state(card.prod, card.unit_info, card.queue_total) : ({
            "enabled": false,
            "reason": ""
        })
    readonly property bool is_enabled: card.card_state.enabled
    readonly property bool is_hovered: recruitMouseArea.containsMouse
    readonly property string display_name: (card.unit_info && card.unit_info.display_name) || card.fallback_name || card.unit_type

    signal recruit_requested(string unit_type)
    signal details_requested(string unit_type, string nation)

    width: 110
    height: 80
    radius: Design.Metrics.radiusMedium
    clip: true
    color: card.panel ? card.panel.recruit_card_color(card.is_enabled, card.is_hovered) : "transparent"
    border.color: card.panel ? card.panel.recruit_card_border(card.is_enabled, card.is_hovered) : "transparent"
    border.width: card.is_hovered && card.is_enabled ? 2 : 1
    opacity: card.is_enabled ? 1 : 0.5
    scale: card.is_hovered && card.is_enabled ? 1.015 : 1

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Recruit %1").arg(card.display_name)
    Accessible.description: card.is_enabled ? card.tooltip_text : card.card_state.reason

    Rectangle {
        id: portraitFrame

        objectName: "recruitPortrait"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: Design.Metrics.space4
        width: height
        radius: Design.Metrics.radiusSmall
        color: Design.Theme.backgroundDeep
        border.width: Design.Metrics.borderThin
        border.color: card.is_enabled ? card.hs.bronzeDeep : Design.Theme.borderSubtle

        Image {
            id: recruitIcon

            objectName: "recruitPortraitImage"
            anchors.fill: parent
            anchors.margins: Design.Metrics.space2
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            source: card.panel ? card.panel.unit_icon_source(card.unit_type, card.prod.nation_id) : ""
            visible: status === Image.Ready
            opacity: card.is_enabled ? 1 : 0.45
        }

        Text {
            anchors.centerIn: parent
            visible: !recruitIcon.visible
            text: card.panel ? card.panel.unit_icon_emoji(card.unit_type) : ""
            color: card.is_enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
            font.pixelSize: Design.Typography.glyphSmall
            opacity: card.is_enabled ? 0.9 : 0.4
        }
    }

    Item {
        id: cardBody

        anchors.left: portraitFrame.right
        anchors.leftMargin: Design.Metrics.space4
        anchors.right: parent.right
        anchors.rightMargin: Design.Metrics.space4
        anchors.top: parent.top
        anchors.topMargin: Design.Metrics.space4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Design.Metrics.space4

        Text {
            id: unitName

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: detailsBadge.width + Design.Metrics.space2
            anchors.top: parent.top
            text: card.display_name
            color: card.is_enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.weight: Design.Typography.bold
            elide: Text.ElideRight
        }

        Flow {
            id: costFlow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: unitName.bottom
            anchors.topMargin: Design.Metrics.space2
            height: Math.min(implicitHeight, parent.height - unitName.height - Design.Metrics.space2)
            clip: true
            spacing: Design.Metrics.space2

            Repeater {
                model: card.panel ? card.panel.cost_entries(card.panel.reserve_cost(card.unit_info), card.unit_info.resource_costs || {}, true) : []

                delegate: Rectangle {
                    id: costPill

                    required property var modelData

                    width: costRow.implicitWidth + Design.Metrics.space4
                    height: costRow.implicitHeight + Design.Metrics.space2
                    radius: height / 2
                    color: card.is_enabled ? "#cc2a1d12" : "#991f150d"
                    border.color: card.is_enabled ? card.hs.bronze : Design.Theme.borderSubtle
                    border.width: Design.Metrics.borderThin

                    Row {
                        id: costRow

                        anchors.centerIn: parent
                        spacing: Design.Metrics.space2

                        Image {
                            width: Design.A11y.scaled(9)
                            height: Design.A11y.scaled(9)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            source: card.panel ? card.panel.cost_icon_source(costPill.modelData.key) : ""
                        }

                        Text {
                            text: costPill.modelData.amount
                            color: card.is_enabled ? Theme.textMain : Theme.textDim
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        id: recruitMouseArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                Design.UiSound.activate();
                card.details_requested(card.unit_type, card.prod.nation_id || "");
                return;
            }
            if (card.is_enabled) {
                Design.UiSound.activate();
                card.recruit_requested(card.unit_type);
            } else {
                Design.UiSound.warning();
            }
        }
        onContainsMouseChanged: {
            if (containsMouse && card.is_enabled)
                Design.UiSound.hover();
        }
        cursorShape: card.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        ToolTip.visible: containsMouse
        ToolTip.text: card.is_enabled ? card.tooltip_text : card.card_state.reason
        ToolTip.delay: 300
    }

    Rectangle {
        id: detailsBadge

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Design.Metrics.space4
        width: Math.max(Design.A11y.scaled(18), detailsLabel.implicitHeight + Design.Metrics.space2)
        height: width
        radius: width / 2
        color: detailsMouse.containsMouse ? Design.Theme.panelLeather : Design.Theme.panelIron
        border.width: Design.Metrics.borderThin
        border.color: detailsMouse.containsMouse ? Design.Theme.accent : Design.Theme.borderStrong

        Text {
            id: detailsLabel

            anchors.centerIn: parent
            text: "i"
            color: Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.bold: true
        }

        MouseArea {
            id: detailsMouse

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                Design.UiSound.activate();
                card.details_requested(card.unit_type, card.prod.nation_id || "");
            }
            ToolTip.visible: containsMouse
            ToolTip.text: qsTr("Show unit details")
            ToolTip.delay: Design.Metrics.tooltipDelay
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#F4E7C8"
        opacity: recruitMouseArea.pressed ? 0.2 : 0
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
