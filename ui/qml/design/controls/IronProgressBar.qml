import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ProgressBar {
    id: control

    property color fillColor: Design.Theme.accent

    implicitHeight: Math.max(8, Design.Metrics.space8)

    background: Rectangle {
        color: Design.Theme.panelIron
        radius: height / 2
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle
    }

    contentItem: Item {
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: control.fillColor

            Behavior on width  {
                NumberAnimation {
                    duration: Design.Motion.fast
                    easing.type: Design.Motion.standardEasing
                }
            }
        }
    }
}
