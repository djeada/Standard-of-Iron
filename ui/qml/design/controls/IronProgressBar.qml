import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ProgressBar {
    id: control

    implicitHeight: 8
    background: Rectangle {
        color: Design.Theme.panelIron
        radius: height / 2
        border.color: Design.Theme.borderSubtle
    }
    contentItem: Item {
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: Design.Theme.accent
        }
    }
}
