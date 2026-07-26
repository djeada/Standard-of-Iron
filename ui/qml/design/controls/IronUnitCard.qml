import QtQuick 2.15
import ".." as Design

Design.IronPanel {
    id: root

    property string unitName: ""
    property string subtitle: ""
    property real health: 1.0
    implicitWidth: Design.Metrics.space24 * 8
    implicitHeight: card.implicitHeight + Design.Metrics.space24
    accessibleName: unitName
    Column {
        id: card

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.Metrics.space4
        Text {
            text: root.unitName
            color: Design.Theme.textPrimary
            font.pixelSize: Design.Typography.label
            font.weight: Design.Typography.medium
        }
        Text {
            text: root.subtitle
            color: Design.Theme.textSecondary
            font.pixelSize: Design.Typography.caption
        }
        IronProgressBar {
            width: parent.width
            value: root.health
        }
    }
}
