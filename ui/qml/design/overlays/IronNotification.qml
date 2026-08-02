import QtQuick 2.15
import ".." as Design

Design.IronPanel {
    id: root

    property string priority: "info"
    property string message: ""
    property string detail: ""
    property string icon: ""
    property int count: 1

    readonly property color priorityColor: priority === "critical" ? Design.Theme.danger : priority === "urgent" ? Design.Theme.warning : priority === "ambient" ? Design.Theme.textSecondary : Design.Theme.accent
    readonly property string priorityGlyph: icon !== "" ? icon : priority === "critical" ? Design.Icons.warning : priority === "urgent" ? Design.Icons.attack : priority === "ambient" ? Design.Icons.ambient : Design.Icons.objective

    signal dismissRequested

    implicitWidth: Design.Metrics.notificationWidth
    implicitHeight: Math.max(Design.Metrics.controlHeight, body.implicitHeight + Design.Metrics.space24)
    raised: true
    border.color: priorityColor
    border.width: priority === "critical" ? Design.Metrics.borderFocus : Design.Metrics.borderThin

    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.message
    Accessible.description: root.detail

    Row {
        id: body

        width: parent.width
        spacing: Design.Metrics.space8

        Text {
            id: rail

            width: Design.Metrics.iconMedium
            text: root.priorityGlyph
            color: root.priorityColor
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.heading
            horizontalAlignment: Text.AlignHCenter
        }

        Column {
            width: body.width - rail.width - Design.Metrics.space8
            spacing: Design.Metrics.space2

            Text {
                width: parent.width
                text: root.count > 1 ? root.message + "  ×" + root.count : root.message
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
                font.weight: root.priority === "critical" ? Design.Typography.bold : Design.Typography.medium
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
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: {
            Design.UiSound.back();
            root.dismissRequested();
        }
    }
}
