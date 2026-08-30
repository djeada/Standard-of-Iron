import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ScrollBar {
    id: control

    readonly property int thickness: Design.Metrics.scrollBarThickness
    readonly property bool overflowing: control.size > 0 && control.size < 1
    readonly property bool lit: control.overflowing && (control.active || control.pressed || control.hovered)

    implicitWidth: control.thickness
    implicitHeight: control.thickness
    minimumSize: 0.15
    padding: 0
    visible: control.overflowing
    policy: control.overflowing ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

    contentItem: Rectangle {
        implicitWidth: control.thickness
        implicitHeight: control.thickness
        radius: Math.min(width, height) / 2
        color: (control.pressed || control.hovered) ? Design.Theme.accent : Design.Theme.borderStrong
        opacity: control.lit ? 1 : 0.8

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.fast
            }
        }

        Behavior on color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }

    background: Rectangle {
        radius: Math.min(width, height) / 2
        color: Design.Theme.backgroundDeep
        opacity: control.lit ? 0.85 : 0.6
    }
}
