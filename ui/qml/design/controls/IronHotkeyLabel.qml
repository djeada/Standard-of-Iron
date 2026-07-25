import QtQuick 2.15
import ".." as Design

Rectangle {
    id: root

    property alias text: label.text
    implicitWidth: label.implicitWidth + Design.Metrics.space8
    implicitHeight: label.implicitHeight + Design.Metrics.space4
    color: Design.Theme.backgroundDeep
    radius: Design.Metrics.radiusSmall
    border.color: Design.Theme.borderSubtle
    Text {
        id: label
        anchors.centerIn: parent
        color: Design.Theme.textSecondary
        font.family: "monospace"
        font.pixelSize: Design.Typography.caption
    }
}
