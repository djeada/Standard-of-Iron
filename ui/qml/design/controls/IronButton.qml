import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Button {
    id: control

    property string tone: "secondary"
    property string disabledReason: ""
    property string accessibleName: text
    readonly property bool destructive: tone === "destructive"
    readonly property bool primary: tone === "primary"

    implicitHeight: Design.Metrics.controlHeight
    implicitWidth: Math.max(112, contentItem.implicitWidth + Design.Metrics.space24 * 2)
    hoverEnabled: true
    Accessible.name: accessibleName
    Accessible.description: enabled ? ToolTip.text : disabledReason
    ToolTip.text: enabled ? "" : disabledReason
    ToolTip.visible: !enabled && hovered && disabledReason.length > 0
    ToolTip.delay: Design.Metrics.tooltipDelay

    contentItem: Text {
        text: control.text
        color: !control.enabled ? Design.Theme.textDisabled : control.destructive ? Design.Theme.danger : Design.Theme.textPrimary
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        font.weight: Design.Typography.medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        color: !control.enabled ? Design.Theme.panelIron : control.down ? Qt.darker(Design.Theme.accent, 1.35) : control.hovered ? Design.Theme.panelLeather : control.primary ? Qt.darker(Design.Theme.accent, 1.65) : Design.Theme.panelIron
        radius: Design.Metrics.radiusSmall
        border.width: control.activeFocus ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: !control.enabled ? Design.Theme.borderSubtle : control.destructive ? Design.Theme.danger : control.activeFocus ? Design.Theme.focus : control.hovered ? Design.Theme.accent : Design.Theme.borderStrong

        Behavior on color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }
}
