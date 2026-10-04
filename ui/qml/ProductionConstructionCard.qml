import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    id: card

    required property var panel
    required property var builder_prod
    required property var spec
    property int cardWidth: 150
    property int cardHeight: 80
    // A narrow card in the builder's two-row bar keeps every cost on show by
    // tightening the chips rather than hiding them.
    readonly property bool compact: cardWidth < 110
    readonly property string item_type: spec.item_type
    readonly property var hs: StyleGuide.historical

    property var construction_info: panel ? panel.get_construction_info(item_type) : ({})
    property var card_state: panel ? panel.construction_card_state(builder_prod, construction_info) : ({
            "enabled": false,
            "reason": ""
        })
    property bool is_enabled: card_state.enabled
    property bool is_hovered: cardMouseArea.containsMouse

    width: cardWidth
    height: cardHeight
    radius: 6
    color: panel ? panel.recruit_card_color(is_enabled, is_hovered) : "transparent"
    border.color: panel ? panel.recruit_card_border(is_enabled, is_hovered) : "transparent"
    border.width: is_hovered && is_enabled ? 2 : 1
    opacity: is_enabled ? 1 : 0.5
    scale: is_hovered && is_enabled ? 1.025 : 1

    Image {
        id: cardIcon

        anchors.fill: parent
        anchors.margins: 6
        fillMode: spec.fit_icon ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        smooth: true
        source: panel ? panel.unit_icon_source(item_type) : ""
        visible: spec.fit_icon ? status === Image.Ready : (source !== "" && status !== Image.Error)
        opacity: parent.is_enabled ? 1 : 0.35
    }

    Text {
        anchors.centerIn: parent
        visible: spec.fit_icon ? cardIcon.status !== Image.Ready : !cardIcon.visible
        text: spec.glyph ? spec.glyph : (panel ? panel.unit_icon_emoji(item_type) : "")
        color: parent.is_enabled ? "#F4E7C8" : "#6B5231"
        font.pixelSize: Design.Typography.glyph
        opacity: parent.is_enabled ? 0.9 : 0.4
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        height: parent.height * 0.7
        radius: 6
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: "#00120A05"
            }
            GradientStop {
                position: 0.45
                color: "#CC120A05"
            }
            GradientStop {
                position: 1.0
                color: "#F2120A05"
            }
        }
    }

    Text {
        anchors.bottom: costFlow.top
        anchors.bottomMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: card.compact && spec.short_label ? spec.short_label : spec.label
        color: parent.is_enabled ? "#D4B57C" : "#6B5231"
        font.pixelSize: Design.Typography.caption
        font.bold: true
        fontSizeMode: card.compact ? Text.HorizontalFit : Text.FixedSize
        minimumPixelSize: Design.Typography.caption - 3
    }

    Flow {
        id: costFlow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: card.compact ? 2 : 4
        spacing: card.compact ? 2 : 4

        Repeater {
            model: panel ? panel.cost_entries(0, card.construction_info.resource_costs || {}, false) : []

            delegate: Rectangle {
                width: costRow.implicitWidth + (card.compact ? 4 : 8)
                height: costRow.implicitHeight + (card.compact ? 2 : 6)
                radius: 8
                color: card.is_enabled ? "#cc2a1d12" : "#991f150d"
                border.color: card.is_enabled ? hs.bronze : "#8C6A3E"
                border.width: 1

                Row {
                    id: costRow

                    anchors.centerIn: parent
                    spacing: card.compact ? 1 : 3

                    Image {
                        width: Design.A11y.scaled(9)
                        height: Design.A11y.scaled(9)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        source: panel.cost_icon_source(modelData.key)
                    }

                    Text {
                        text: modelData.amount
                        color: card.is_enabled ? Theme.textMain : Theme.textDim
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                    }
                }
            }
        }
    }

    MouseArea {
        id: cardMouseArea

        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (parent.is_enabled) {
                panel.builder_construction(item_type);
            } else {
                Design.UiSound.warning();
            }
        }
        cursorShape: parent.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        ToolTip.visible: containsMouse
        ToolTip.text: !panel ? "" : parent.is_enabled ? spec.tooltip.arg(spec.description).arg(panel.format_cost_summary(0, card.construction_info.resource_costs || {}, qsTr("reserve"))).arg((card.construction_info.build_time || spec.default_build_time).toFixed(0)) : card.card_state.reason
        ToolTip.delay: 300
    }

    Rectangle {
        anchors.fill: parent
        color: "#F4E7C8"
        opacity: cardMouseArea.pressed ? 0.2 : 0
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
