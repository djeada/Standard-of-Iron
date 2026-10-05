import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

TextField {
    id: control

    placeholderText: Design.Icons.search + "  " + qsTr("Search")
    color: Design.Theme.textPrimary
    placeholderTextColor: Design.Theme.textDisabled
    selectByMouse: true
    hoverEnabled: true
    implicitHeight: Design.Metrics.controlHeight
    leftPadding: Design.Metrics.space8
    background: Rectangle {
        color: Design.Theme.panelIron
        radius: Design.Metrics.radiusSmall
        border.width: control.activeFocus ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.activeFocus ? Design.Theme.focus : control.hovered ? Design.Theme.accent : Design.Theme.borderSubtle

        Behavior on border.color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }
}
