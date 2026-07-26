import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Slider {
    id: control

    property string accessibleName: ""
    readonly property bool showFocusRing: visualFocus || (Design.A11y.alwaysShowFocus && activeFocus)

    implicitHeight: Math.max(Design.Metrics.controlHeight, Design.Metrics.minTouchTarget)
    implicitWidth: Design.Metrics.space24 * 6

    Accessible.name: accessibleName
    Accessible.description: Math.round(control.position * 100) + "%"

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: Design.Metrics.space4
        radius: height / 2
        color: Design.Theme.panelIron
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle

        Rectangle {
            width: control.position * parent.width
            height: parent.height
            radius: parent.radius
            color: control.enabled ? Design.Theme.accent : Design.Theme.textDisabled
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: Design.Metrics.iconSmall
        height: width
        radius: Design.Metrics.radiusSmall
        color: !control.enabled ? Design.Theme.surfaceDisabled : control.pressed ? Design.Theme.focus : Design.Theme.accent
        border.width: control.showFocusRing ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.showFocusRing ? Design.Theme.focus : Design.Theme.backgroundDeep
    }
}
