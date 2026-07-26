import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

CheckBox {
    id: control

    property string description: ""
    property string accessibleName: text
    readonly property bool showFocusRing: visualFocus || (Design.A11y.alwaysShowFocus && activeFocus)

    implicitHeight: Math.max(Design.Metrics.controlHeight, Design.Metrics.minTouchTarget)
    hoverEnabled: true
    spacing: Design.Metrics.space8
    Accessible.name: accessibleName
    Accessible.description: description

    indicator: Rectangle {
        x: 0
        y: (control.height - height) / 2
        implicitWidth: Design.Metrics.iconSmall
        implicitHeight: Design.Metrics.iconSmall
        radius: Design.Metrics.radiusSmall
        color: control.checked ? Design.Theme.accent : Design.Theme.panelIron
        border.width: control.showFocusRing ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: !control.enabled ? Design.Theme.borderSubtle : control.showFocusRing ? Design.Theme.focus : control.hovered ? Design.Theme.accent : Design.Theme.borderStrong

        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: Design.Theme.backgroundDeep
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.weight: Design.Typography.bold
        }

        Behavior on color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
