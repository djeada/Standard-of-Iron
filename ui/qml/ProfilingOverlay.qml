import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: profiling_overlay

    readonly property bool available: typeof profiling_hud !== 'undefined' && profiling_hud !== null
    property bool visible_overlay: available && profiling_hud.enabled

    objectName: "ProfilingOverlay"

    function toggle() {
        if (!available)
            return;
        profiling_hud.enabled = !profiling_hud.enabled;
        visible_overlay = profiling_hud.enabled;
    }

    Shortcut {
        sequence: "F10"
        context: Qt.ApplicationShortcut
        onActivated: profiling_overlay.toggle()
    }

    Rectangle {
        id: panel

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 12
        width: readout.implicitWidth + 20
        height: readout.implicitHeight + 16
        radius: 4
        color: "#cc0b0d10"
        border.color: "#40ffffff"
        border.width: 1
        visible: profiling_overlay.visible_overlay

        Text {
            id: readout

            anchors.centerIn: parent
            text: profiling_overlay.available ? profiling_hud.overlay_text : ""
            color: "#e8f0f4"
            font.family: "monospace"
            font.pixelSize: Design.Typography.caption
            textFormat: Text.PlainText
        }
    }
}
