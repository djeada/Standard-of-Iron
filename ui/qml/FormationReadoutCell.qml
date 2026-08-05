import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: cell

    property string label: ""
    property string value: ""
    property color tone: Design.Theme.textPrimary

    implicitHeight: caption.implicitHeight + amount.implicitHeight
    height: implicitHeight

    Text {
        id: caption

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Design.Theme.textSecondary
        elide: Text.ElideRight
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.caption
        text: cell.label
    }

    Text {
        id: amount

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: caption.bottom
        color: cell.tone
        elide: Text.ElideRight
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        font.weight: Design.Typography.medium
        text: cell.value
    }
}
