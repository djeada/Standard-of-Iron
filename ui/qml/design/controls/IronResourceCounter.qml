import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Row {
    id: root

    property url iconSource: ""

    property string iconText: ""

    property string label: ""
    property int amount: 0

    property string amountText: ""

    property int trend: 0

    property string status: "default"
    property bool compact: false

    property string tooltipText: ""

    readonly property string effectiveTooltip: root.tooltipText !== "" ? root.tooltipText : (root.label !== "" ? root.label + ": " + root.displayText : "")

    readonly property string displayText: amountText !== "" ? amountText : amount.toString()
    readonly property color valueColor: root.status === "default" ? Design.Theme.textPrimary : Design.Theme.statusColor(root.status)

    spacing: Design.Metrics.space4

    Accessible.role: Accessible.StaticText
    Accessible.name: root.label
    Accessible.description: root.displayText

    ToolTip.visible: hover.hovered && root.effectiveTooltip !== ""
    ToolTip.delay: Design.Metrics.tooltipDelay
    ToolTip.text: root.effectiveTooltip

    HoverHandler {
        id: hover
    }

    Item {
        width: root.compact ? Design.Metrics.iconSmall : Design.Metrics.iconMedium
        height: width
        anchors.verticalCenter: parent.verticalCenter

        Image {
            id: art

            anchors.fill: parent
            source: root.iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            visible: source.toString() !== "" && status === Image.Ready
        }

        Text {
            anchors.centerIn: parent
            visible: !art.visible
            text: root.iconText !== "" ? root.iconText : Design.Icons.objective
            color: Design.Theme.accent
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.body
        }
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: root.displayText
        color: root.valueColor
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.body
        font.weight: Design.Typography.bold
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        visible: root.trend !== 0
        text: (root.trend > 0 ? "+" : "") + root.trend
        color: root.trend >= 0 ? Design.Theme.success : Design.Theme.danger
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.caption
    }
}
