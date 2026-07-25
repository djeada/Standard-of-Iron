import QtQuick 2.15
import ".." as Design

IronButton {
    id: control

    property string iconText: ""
    property string tooltip: ""

    text: iconText
    implicitWidth: Design.Metrics.controlHeight
    accessibleName: tooltip
    ToolTip.text: tooltip
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.delay: Design.Metrics.tooltipDelay
}
