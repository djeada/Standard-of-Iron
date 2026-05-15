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

    Row {
        id: abilityRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 72
        spacing: 8

        Repeater {
            model: [{
                    "key": "LMB",
                    "label": "Strike",
                    "ready": "has_commander"
                }, {
                    "key": "RMB",
                    "label": "Guard",
                    "ready": "has_commander"
                }, {
                    "key": "1",
                    "label": "Rush",
                    "ready": "vanguard_rush_ready",
                    "remaining": "vanguard_rush_cooldown_remaining",
                    "total": "vanguard_rush_cooldown"
                }, {
                    "key": "2",
                    "label": "Wind",
                    "ready": "second_wind_ready",
                    "remaining": "second_wind_cooldown_remaining",
                    "total": "second_wind_cooldown"
                }, {
                    "key": "F",
                    "label": "Bash",
                    "ready": "shield_bash_ready",
                    "remaining": "shield_bash_cooldown_remaining",
                    "total": "shield_bash_cooldown"
                }, {
                    "key": "R",
                    "label": "Rally",
                    "ready": "rally_ready",
                    "remaining": "rally_cooldown_remaining",
                    "total": "rally_cooldown"
                }]
            delegate: Rectangle {
                width: 52
                height: 52
                radius: 6
                property bool ready: root.status_value(modelData["ready"], true)
                property real cooldownRatio: root.cooldown_ratio(modelData["remaining"] || "", modelData["total"] || "")
                color: ready ? "#1a1a1a" : "#101010"
                opacity: ready ? 0.82 : 0.58
                border.color: ready ? "#777777" : "#444444"
                border.width: ready ? 1 : 2

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: parent.height * parent.cooldownRatio
                    radius: parent.radius
                    color: "#aa000000"
                    visible: parent.cooldownRatio > 0.01
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        text: modelData["key"]
                        color: parent.parent.ready ? "#ffffff" : "#888888"
                        font.pixelSize: 11
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: modelData["label"]
                        color: parent.parent.ready ? "#aaaaaa" : "#777777"
                        font.pixelSize: 9
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: Math.ceil(Number(root.status_value(modelData["remaining"] || "", 0.0))).toString()
                    visible: parent.cooldownRatio > 0.01
                    color: "#ffffff"
                    font.pixelSize: 13
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#aa000000"
                }
            }
        }
    }
}
