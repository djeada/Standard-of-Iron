import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: topRoot

    property bool gameIsPaused: false
    property real currentSpeed: 1
    readonly property int barMinHeight: 72
    readonly property bool compact: width < 800
    readonly property bool ultraCompact: width < 560

    signal pauseToggled()
    signal speedChanged(real speed)

    Rectangle {
        id: topPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: barMinHeight
        color: "#1a1a1a"
        opacity: 0.98
        clip: true

        Rectangle {
            anchors.fill: parent
            opacity: 0.9

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#22303a"
                }

                GradientStop {
                    position: 1
                    color: "#0f1a22"
                }

            }

        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "transparent"
                }

                GradientStop {
                    position: 0.5
                    color: "#3498db"
                }

                GradientStop {
                    position: 1
                    color: "transparent"
                }

            }

        }

        RowLayout {
            id: barRow

            anchors.fill: parent
            anchors.margins: 8
            spacing: 12

            RowLayout {
                id: leftGroup

                spacing: 10
                Layout.alignment: Qt.AlignVCenter

                Button {
                    id: pauseBtn

                    Layout.preferredWidth: topRoot.compact ? 48 : 56
                    Layout.preferredHeight: Math.min(40, topPanel.height - 12)
                    text: topRoot.gameIsPaused ? "\u25B6" : "\u23F8"
                    font.pixelSize: 26
                    font.bold: true
                    focusPolicy: Qt.NoFocus
                    onClicked: topRoot.pauseToggled()

                    background: Rectangle {
                        color: parent.pressed ? "#e74c3c" : parent.hovered ? "#c0392b" : "#34495e"
                        radius: 6
                        border.color: "#2c3e50"
                        border.width: 1
                    }

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#ecf0f1"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                }

                Rectangle {
                    width: 2
                    Layout.fillHeight: true
                    radius: 1
                    visible: !topRoot.compact

                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 0.5
                            color: "#34495e"
                        }

                        GradientStop {
                            position: 1
                            color: "transparent"
                        }

                    }

                }

                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignVCenter

                    Label {
                        text: "Speed:"
                        visible: !topRoot.compact
                        color: "#ecf0f1"
                        font.pixelSize: 14
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    Row {
                        id: speedRow

                        property var options: [0.5, 1, 2]

                        spacing: 8
                        visible: !topRoot.compact

                        ButtonGroup {
                            id: speedGroup
                        }

                        Repeater {
                            model: speedRow.options

                            delegate: Button {
                                Layout.minimumWidth: 48
                                width: 56
                                height: Math.min(34, topPanel.height - 16)
                                checkable: true
                                enabled: !topRoot.gameIsPaused
                                checked: (topRoot.currentSpeed === modelData) && !topRoot.gameIsPaused
                                focusPolicy: Qt.NoFocus
                                text: modelData + "x"
                                ButtonGroup.group: speedGroup
                                onClicked: topRoot.speedChanged(modelData)

                                background: Rectangle {
                                    color: parent.checked ? "#27ae60" : parent.hovered ? "#34495e" : "#2c3e50"
                                    radius: 6
                                    border.color: parent.checked ? "#229954" : "#1a252f"
                                    border.width: 1
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                            }

                        }

                    }

                    ComboBox {
                        id: speedCombo

                        visible: topRoot.compact
                        Layout.preferredWidth: 120
                        model: ["0.5x", "1x", "2x"]
                        currentIndex: topRoot.currentSpeed === 0.5 ? 0 : topRoot.currentSpeed === 1 ? 1 : 2
                        enabled: !topRoot.gameIsPaused
                        font.pixelSize: 13
                        onActivated: function(i) {
                            var v = i === 0 ? 0.5 : (i === 1 ? 1 : 2);
                            topRoot.speedChanged(v);
                        }
                    }

                }

                Rectangle {
                    width: 2
                    Layout.fillHeight: true
                    radius: 1
                    visible: !topRoot.compact

                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 0.5
                            color: "#34495e"
                        }

                        GradientStop {
                            position: 1
                            color: "transparent"
                        }

                    }

                }

                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignVCenter

                    Label {
                        text: "Camera:"
                        visible: !topRoot.compact
                        color: "#ecf0f1"
                        font.pixelSize: 14
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    Button {
                        id: followBtn

                        Layout.preferredWidth: topRoot.compact ? 44 : 80
                        Layout.preferredHeight: Math.min(34, topPanel.height - 16)
                        checkable: true
                        text: topRoot.compact ? "\u2609" : "Follow"
                        font.pixelSize: 13
                        focusPolicy: Qt.NoFocus
                        onToggled: {
                            if (typeof game !== 'undefined' && game.cameraFollowSelection)
                                game.cameraFollowSelection(checked);

                        }

                        background: Rectangle {
                            color: parent.checked ? "#3498db" : parent.hovered ? "#34495e" : "#2c3e50"
                            radius: 6
                            border.color: parent.checked ? "#2980b9" : "#1a252f"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "#ecf0f1"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                    Button {
                        id: resetBtn

                        Layout.preferredWidth: topRoot.compact ? 44 : 80
                        Layout.preferredHeight: Math.min(34, topPanel.height - 16)
                        text: topRoot.compact ? "\u21BA" : "Reset"
                        font.pixelSize: 13
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            if (typeof game !== 'undefined' && game.resetCamera)
                                game.resetCamera();

                        }

                        background: Rectangle {
                            color: parent.hovered ? "#34495e" : "#2c3e50"
                            radius: 6
                            border.color: "#1a252f"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "#ecf0f1"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                }

            }

            Item {
                Layout.fillWidth: true
            }

            RowLayout {
                id: rightGroup

                spacing: 12
                Layout.alignment: Qt.AlignVCenter

                Row {
                    id: statsRow

                    spacing: 10
                    Layout.alignment: Qt.AlignVCenter

                    Label {
                        id: playerLbl

                        text: "🗡️ " + (typeof game !== 'undefined' ? game.playerTroopCount : 0) + " / " + (typeof game !== 'undefined' ? game.maxTroopsPerPlayer : 0)
                        color: {
                            if (typeof game === 'undefined')
                                return "#95a5a6";

                            var count = game.playerTroopCount;
                            var max = game.maxTroopsPerPlayer;
                            if (count >= max)
                                return "#e74c3c";

                            if (count >= max * 0.8)
                                return "#f39c12";

                            return "#2ecc71";
                        }
                        font.pixelSize: 14
                        font.bold: true
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Rectangle {
                        width: 2
                        height: 24
                        color: "#34495e"
                        opacity: 0.5
                        visible: !topRoot.compact
                    }

                    Label {
                        id: ownersLbl

                        text: {
                            if (typeof game === 'undefined')
                                return "Players: 0";

                            var owners = game.ownerInfo;
                            var playerCount = 0;
                            var aiCount = 0;
                            for (var i = 0; i < owners.length; i++) {
                                if (owners[i].type === "Player")
                                    playerCount++;
                                else if (owners[i].type === "AI")
                                    aiCount++;
                            }
                            return "👥 " + playerCount + " | 🤖 " + aiCount;
                        }
                        color: "#ecf0f1"
                        font.pixelSize: 13
                        font.bold: false
                        visible: !topRoot.compact
                        verticalAlignment: Text.AlignVCenter
                        ToolTip.visible: ma.containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: {
                            if (typeof game === 'undefined')
                                return "";

                            var owners = game.ownerInfo;
                            var tip = "Owner IDs:\n";
                            for (var i = 0; i < owners.length; i++) {
                                tip += owners[i].id + ": " + owners[i].name + " (" + owners[i].type + ")";
                                if (owners[i].isLocal)
                                    tip += " [You]";

                                tip += "\n";
                            }
                            return tip;
                        }

                        MouseArea {
                            id: ma

                            anchors.fill: parent
                            hoverEnabled: true
                        }

                    }

                    Label {
                        id: enemyLbl

                        text: "💀 " + (typeof game !== 'undefined' ? game.enemyTroopsDefeated : 0)
                        color: "#ecf0f1"
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                }

                Item {
                    id: miniWrap

                    visible: !topRoot.ultraCompact
                    Layout.preferredWidth: Math.round(topPanel.height * 2.2)
                    Layout.minimumWidth: Math.round(topPanel.height * 1.6)
                    Layout.preferredHeight: topPanel.height - 8

                    Rectangle {
                        anchors.fill: parent
                        color: "#0f1a22"
                        radius: 8
                        border.width: 2
                        border.color: "#3498db"

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 3
                            radius: 6
                            color: "#0a0f14"

                            Label {
                                anchors.centerIn: parent
                                text: "MINIMAP"
                                color: "#3f5362"
                                font.pixelSize: 12
                                font.bold: true
                            }

                        }

                    }

                }

            }

        }

    }

}
