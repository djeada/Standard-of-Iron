import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Design.IronButton {
    id: control

    property string iconText: ""
    property string tooltip: ""

    text: iconText
    implicitWidth: Math.max(Design.Metrics.controlHeight, Design.Metrics.minTouchTarget)
    accessibleName: tooltip
    ToolTip.text: enabled ? tooltip : (disabledReason !== "" ? disabledReason : tooltip)
    ToolTip.visible: hovered && ToolTip.text.length > 0
    ToolTip.delay: Design.Metrics.tooltipDelay
}
