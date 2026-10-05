import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ProgressBar {
    id: control

    property color fillColor: Design.Theme.accent

    property real animatedPosition: control.visualPosition

    implicitHeight: Math.max(8, Design.Metrics.space8)

    Behavior on animatedPosition  {
        NumberAnimation {
            duration: Design.Motion.fast
            easing.type: Design.Motion.standardEasing
        }
    }

    background: Rectangle {
        color: Design.Theme.backgroundDeep
        radius: height / 2
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle
    }

    contentItem: Item {
        Rectangle {
            width: control.animatedPosition * parent.width
            height: parent.height
            radius: height / 2
            visible: width > 0

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Design.Theme.sheenTop(control.fillColor)
                }

                GradientStop {
                    position: 1
                    color: Design.Theme.sheenBottom(control.fillColor)
                }
            }
        }
    }
}
