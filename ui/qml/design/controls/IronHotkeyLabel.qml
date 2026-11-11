import QtQuick 2.15
import ".." as Design

Rectangle {
    id: root

    property alias text: label.text
    implicitWidth: Math.max(label.implicitWidth + Design.Metrics.space8, label.implicitHeight + Design.Metrics.space2)
    implicitHeight: label.implicitHeight + Design.Metrics.space2
    color: Qt.rgba(0, 0, 0, 0.28)
    radius: Design.Metrics.radiusSmall
    border.width: 0

    Text {
        id: label
        anchors.centerIn: parent
        color: Qt.rgba(Design.Theme.accent.r, Design.Theme.accent.g, Design.Theme.accent.b, 0.85)
        font.family: Design.Typography.family
        font.weight: Design.Typography.bold
        font.pixelSize: Math.max(10, Math.round(Design.Typography.caption * 0.85))
    }
}
