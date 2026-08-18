import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property string objectiveState: "active"
    property string objectiveText: ""
    property string detail: ""

    property real progress: -1

    readonly property string marker: objectiveState === "complete" ? "✓" : objectiveState === "failed" ? "✕" : objectiveState === "current" ? "▶" : objectiveState === "optional" ? Design.Icons.objective : "◇"
    readonly property color tone: objectiveState === "complete" ? Design.Theme.success : objectiveState === "failed" ? Design.Theme.danger : objectiveState === "current" || objectiveState === "optional" ? Design.Theme.accent : Design.Theme.textSecondary

    implicitHeight: body.implicitHeight + Design.Metrics.space16
    implicitWidth: Design.Metrics.space24 * 10

    Accessible.role: Accessible.StaticText
    Accessible.name: root.objectiveText
    Accessible.description: root.objectiveState

    Rectangle {
        anchors.fill: parent
        radius: Design.Metrics.radiusSmall
        color: Design.Theme.panelIron
        border.width: root.objectiveState === "current" ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: root.tone
        opacity: root.objectiveState === "complete" ? 0.7 : 1
    }

    Row {
        id: body

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Design.Metrics.space8
        anchors.rightMargin: Design.Metrics.space8
        spacing: Design.Metrics.space8

        Text {
            width: Design.Metrics.iconSmall
            text: root.marker
            color: root.tone
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.body
            font.weight: Design.Typography.bold
        }

        Column {
            width: Math.max(0, body.width - Design.Metrics.iconSmall - Design.Metrics.space8)
            spacing: Design.Metrics.space2

            Text {
                width: parent.width
                text: root.objectiveText
                color: root.objectiveState === "failed" ? Design.Theme.danger : Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.strikeout: root.objectiveState === "complete"
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                visible: root.detail !== ""
                text: root.detail
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                wrapMode: Text.WordWrap
            }

            Design.IronProgressBar {
                width: parent.width
                visible: root.progress >= 0
                value: Math.max(0, Math.min(1, root.progress))
                fillColor: root.tone
            }
        }
    }
}
