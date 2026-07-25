import QtQuick 2.15
import ".." as Design

Rectangle {
    id: root

    property alias text: label.text
    property color tone: Design.Theme.accent

    implicitWidth: label.implicitWidth + Design.Metrics.space12
    implicitHeight: label.implicitHeight + Design.Metrics.space4
    radius: height / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.18)
    border.color: tone
    Text {
        id: label
        anchors.centerIn: parent
        color: root.tone
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.caption
        font.weight: Design.Typography.medium
    }
}
