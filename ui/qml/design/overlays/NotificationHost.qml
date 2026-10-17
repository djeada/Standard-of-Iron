import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property int visibleCount: 3
    property int alignment: Qt.AlignRight
    property real maxHeight: -1

    readonly property var entries: Design.Notifications.queue.slice(0, Math.max(1, visibleCount))
    readonly property int fittingCount: root.count_fitting(rowsRevision)
    property int rowsRevision: 0

    function count_fitting(revision) {
        var total = rows.count;
        if (root.maxHeight < 0)
            return total;
        var used = 0;
        var shown = 0;
        for (var i = 0; i < total; ++i) {
            var row = rows.itemAt(i);
            if (!row)
                break;
            var next = used + (shown > 0 ? column.spacing : 0) + row.implicitHeight;
            if (next > root.maxHeight)
                break;
            used = next;
            shown += 1;
        }
        return shown;
    }

    implicitWidth: Design.Metrics.notificationWidth
    implicitHeight: column.implicitHeight
    width: implicitWidth
    height: implicitHeight

    Column {
        id: column

        width: parent.width
        spacing: Design.Metrics.space8

        Repeater {
            id: rows

            model: root.entries
            onItemAdded: root.rowsRevision += 1
            onItemRemoved: root.rowsRevision += 1

            delegate: Design.IronNotification {
                id: notificationDelegate
                required property var modelData
                required property int index
                readonly property bool fits: notificationDelegate.index < root.fittingCount
                readonly property alias dwelling: dwellTimer.running

                objectName: "notificationRow"
                width: column.width
                visible: notificationDelegate.fits
                priority: modelData.priority
                message: modelData.message
                detail: modelData.detail
                icon: modelData.icon
                count: modelData.repeats
                onDismissRequested: Design.Notifications.dismiss(modelData.id)

                Timer {
                    id: dwellTimer

                    running: notificationDelegate.fits && !notificationDelegate.modelData.sticky
                    interval: Design.Motion.dwellFor(notificationDelegate.modelData.priority)
                    onTriggered: Design.Notifications.dismiss(notificationDelegate.modelData.id)
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
