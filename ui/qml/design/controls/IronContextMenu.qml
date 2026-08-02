import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Menu {
    id: root

    padding: Design.Metrics.space4
    onOpened: Design.UiSound.panelOpen()
    onClosed: Design.UiSound.panelClose()

    background: Rectangle {
        color: Design.Theme.backgroundRaised
        border.color: Design.Theme.borderStrong
        radius: Design.Metrics.radiusSmall
    }
}
