import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: card

    property var panel: null
    property var prod: ({})
    property string unit_type: ""
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

    signal recruit_requested(string unit_type)
    signal details_requested(string unit_type, string nation)

    width: 110
    height: 80
    radius: 6
    color: card.panel ? card.panel.recruit_card_color(card.is_enabled, card.is_hovered) : "transparent"
    border.color: card.panel ? card.panel.recruit_card_border(card.is_enabled, card.is_hovered) : "transparent"
    border.width: card.is_hovered && card.is_enabled ? 2 : 1
    opacity: card.is_enabled ? 1 : 0.5
    scale: card.is_hovered && card.is_enabled ? 1.025 : 1

    Image {
        id: recruitIcon

        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        smooth: true
        source: card.panel ? card.panel.unit_icon_source(card.unit_type, card.prod.nation_id) : ""
        visible: source !== ""
        opacity: card.is_enabled ? 1 : 0.35
    }

    Text {
        anchors.centerIn: parent
        visible: !recruitIcon.visible
        text: card.panel ? card.panel.unit_icon_emoji(card.unit_type) : ""
        color: card.is_enabled ? "#F4E7C8" : "#6B5231"
        font.pixelSize: Design.Typography.glyphLarge
        opacity: card.is_enabled ? 0.9 : 0.4
    }

    Flow {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 3
        spacing: 3

        Repeater {
            model: card.panel ? card.panel.cost_entries(card.panel.reserve_cost(card.unit_info), card.unit_info.resource_costs || {}, true) : []

            delegate: Rectangle {
                id: costPill

                required property var modelData

                width: costRow.implicitWidth + 6
                height: costRow.implicitHeight + 4
                radius: 6
                color: card.is_enabled ? "#cc2a1d12" : "#991f150d"
                border.color: card.is_enabled ? card.hs.bronze : "#8C6A3E"
                border.width: 1

                Row {
                    id: costRow

                    anchors.centerIn: parent
                    spacing: 3

                    Image {
                        width: Design.A11y.scaled(8)
                        height: Design.A11y.scaled(8)
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
