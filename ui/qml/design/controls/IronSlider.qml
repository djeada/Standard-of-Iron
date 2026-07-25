import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Slider {
    id: control

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: Design.Theme.borderSubtle
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Design.Theme.accent
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: control.pressed ? Design.Theme.selection : Design.Theme.accent
        border.color: control.activeFocus ? Design.Theme.focus : Design.Theme.backgroundDeep
        border.width: 2
    }
}
