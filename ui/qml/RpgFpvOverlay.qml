import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

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
                    "label": "Strike"
                }, {
                    "key": "RMB",
                    "label": "Guard"
                }, {
                    "key": "F",
                    "label": "Special"
                }, {
                    "key": "R",
                    "label": "Rally"
                }]
            delegate: Rectangle {
                width: 52
                height: 52
                radius: 6
                color: "#1a1a1a"
                opacity: 0.78
                border.color: "#555555"
                border.width: 1

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        text: modelData["key"]
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: modelData["label"]
                        color: "#aaaaaa"
                        font.pixelSize: 9
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }
}
