import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

TabBar {
    id: root

    background: Rectangle {
        color: Design.Theme.backgroundRaised
        border.color: Design.Theme.borderSubtle
    }

    delegate: TabButton {
        required property string modelData
        text: modelData
        contentItem: Text {
            text: parent.text
            color: parent.checked ? Design.Theme.textPrimary : Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.label
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: parent.checked ? Design.Theme.panelIron : "transparent"
            border.width: parent.activeFocus ? Design.Metrics.borderFocus : 0
            border.color: Design.Theme.focus
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: Design.Theme.accent
                visible: parent.parent.checked
            }
        }
    }
}
