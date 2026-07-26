import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property int visibleCount: 3
    property int alignment: Qt.AlignRight

    readonly property var entries: Design.Notifications.queue.slice(0, Math.max(1, visibleCount))

    implicitWidth: Design.Metrics.notificationWidth
    implicitHeight: column.implicitHeight
    width: implicitWidth
    height: implicitHeight

    Column {
        id: column

        width: parent.width
        spacing: Design.Metrics.space8

        Repeater {
            model: root.entries

            delegate: Design.IronNotification {
                required property var modelData

                width: column.width
                priority: modelData.priority
                message: modelData.message
                detail: modelData.detail
                icon: modelData.icon
                count: modelData.repeats
                onDismissRequested: Design.Notifications.dismiss(modelData.id)

                Timer {
                    running: !modelData.sticky
                    interval: Design.Motion.dwellFor(modelData.priority)
                    onTriggered: Design.Notifications.dismiss(modelData.id)
                }

                opacity: 0
                Component.onCompleted: opacity = 1
                Behavior on opacity  {
                    NumberAnimation {
                        duration: Design.Motion.fast
                        easing.type: Design.Motion.standardEasing
                    }
                }
            }
        }
    }
}
