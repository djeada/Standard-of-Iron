import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    property var status: ({})

    function status_value(key, fallback) {
        if (!status || status[key] === undefined || status[key] === null) {
            return fallback;
        }
        return status[key];
    }

    function cooldown_ratio(remainingKey, totalKey) {
        var total = Number(status_value(totalKey, 0.0));
        if (total <= 0.0) {
            return 0.0;
        }
        return Math.max(0.0, Math.min(1.0, Number(status_value(remainingKey, 0.0)) / total));
    }

    Timer {
        interval: 80
        repeat: true
        running: root.visible && typeof game !== 'undefined' && game.get_controlled_commander_status
        onTriggered: root.status = game.get_controlled_commander_status()
    }

    Component.onCompleted: {
        if (typeof game !== 'undefined' && game.get_controlled_commander_status) {
            status = game.get_controlled_commander_status();
        }
    }

    Item {
        id: crosshair
        width: 32
        height: 32
        anchors.centerIn: parent

        Rectangle {
            width: 4
            height: 4
            radius: 2
            color: "#e0e0e0"
            opacity: 0.90
            anchors.centerIn: parent
        }

        Rectangle {
            width: 2
            height: 10
            color: "#e0e0e0"
            opacity: 0.80
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 5
        }

        Rectangle {
            width: 2
            height: 10
            color: "#e0e0e0"
            opacity: 0.80
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom
            anchors.topMargin: 5
        }

        Rectangle {
            width: 10
            height: 2
            color: "#e0e0e0"
            opacity: 0.80
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.left
            anchors.rightMargin: 5
        }

        Rectangle {
            width: 10
            height: 2
            color: "#e0e0e0"
            opacity: 0.80
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.right
            anchors.leftMargin: 5
        }
    }
}
