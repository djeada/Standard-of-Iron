import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Design.IronButton {
    id: control

    property string button_style: "primary"
    property bool ui_sound_enabled: true

    uiSound: ui_sound_enabled ? (checkable ? "toggle" : "click") : "none"
    hoverSound: ui_sound_enabled
    tone: button_style === "primary" ? "primary" : button_style === "danger" ? "destructive" : "secondary"

    implicitHeight: button_style === "small" ? Math.max(Design.Metrics.controlHeight - Design.Metrics.space8, Design.Metrics.minTouchTarget) : Design.Metrics.controlHeight
    implicitWidth: Math.max(button_style === "small" ? Design.Metrics.space24 * 3 : Design.Metrics.space24 * 5, contentItem.implicitWidth + Design.Metrics.space24 * 2)

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: control.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
