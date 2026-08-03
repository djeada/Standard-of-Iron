import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ItemDelegate {
    id: control

    implicitHeight: 42
    hoverEnabled: true

    property bool blocked: false
    readonly property bool interactive: enabled && !blocked

    Connections {
        function onClicked() {
            Design.UiSound.activate();
        }

        function onHoveredChanged() {
            if (control.hovered && control.interactive)
                Design.UiSound.hover();
        }

        target: control
    }

    MouseArea {
        anchors.fill: parent
        enabled: control.blocked
        visible: enabled
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.ForbiddenCursor
        onPressed: Design.UiSound.warning()
    }

    contentItem: Text {
        text: control.text
        color: control.interactive ? Design.Theme.textPrimary : Design.Theme.textDisabled
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        color: control.checked ? Design.Theme.selection : control.hovered ? Design.Theme.panelLeather : Design.Theme.panelIron
        border.width: control.activeFocus ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.activeFocus ? Design.Theme.focus : Design.Theme.borderSubtle
    }
}
