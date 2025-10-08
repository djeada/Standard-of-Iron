import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: topRoot
    property bool gameIsPaused: false
    property real currentSpeed: 1.0
    signal pauseToggled()
    signal speedChanged(real speed)

    Rectangle {
        id: topPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(50, parent.height * 0.08)
        color: "#1a1a1a"
        opacity: 0.95

        
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#2c3e50" }
                GradientStop { position: 1.0; color: "#1a252f" }
            }
            opacity: 0.8
        }

        
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: "#3498db" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 15

            
            Button {
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                text: topRoot.gameIsPaused ? "▶" : "⏸"
                font.pointSize: 18
                font.bold: true
                focusPolicy: Qt.NoFocus
                background: Rectangle {
                    color: parent.pressed ? "#e74c3c" : (parent.hovered ? "#c0392b" : "#34495e")
                    radius: 4
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
                onClicked: {
                    
                    topRoot.pauseToggled()
                }
            }

            Rectangle { width: 2; Layout.fillHeight: true; Layout.topMargin: 8; Layout.bottomMargin: 8
                gradient: Gradient { GradientStop { position: 0.0; color: "transparent" } GradientStop { position: 0.5; color: "#34495e" } GradientStop { position: 1.0; color: "transparent" } }
            }

            
            Row {
                Layout.preferredWidth: 220
                spacing: 8

                Text { text: "Speed:"; color: "#ecf0f1"; font.pointSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }

                ButtonGroup { id: speedGroup }

                Button {
                    width: 50; height: 32; text: "0.5x"
                    enabled: !topRoot.gameIsPaused
                    checkable: true
                    checked: topRoot.currentSpeed === 0.5 && !topRoot.gameIsPaused
                    focusPolicy: Qt.NoFocus
                    ButtonGroup.group: speedGroup
                    background: Rectangle { color: parent.checked ? "#27ae60" : (parent.hovered ? "#34495e" : "#2c3e50"); radius: 4; border.color: parent.checked ? "#229954" : "#1a252f"; border.width: 1 }
                    contentItem: Text { text: parent.text; font.pointSize: 9; font.bold: true; color: parent.enabled ? "#ecf0f1" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: { topRoot.speedChanged(0.5) }
                }

                Button { width: 50; height: 32; text: "1x"; enabled: !topRoot.gameIsPaused; checkable: true; checked: topRoot.currentSpeed === 1.0 && !topRoot.gameIsPaused; focusPolicy: Qt.NoFocus; ButtonGroup.group: speedGroup
                    background: Rectangle { color: parent.checked ? "#27ae60" : (parent.hovered ? "#34495e" : "#2c3e50"); radius: 4; border.color: parent.checked ? "#229954" : "#1a252f"; border.width: 1 }
                    contentItem: Text { text: parent.text; font.pointSize: 9; font.bold: true; color: parent.enabled ? "#ecf0f1" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: { topRoot.speedChanged(1.0) }
                }

                Button { width: 50; height: 32; text: "2x"; enabled: !topRoot.gameIsPaused; checkable: true; checked: topRoot.currentSpeed === 2.0 && !topRoot.gameIsPaused; focusPolicy: Qt.NoFocus; ButtonGroup.group: speedGroup
                    background: Rectangle { color: parent.checked ? "#27ae60" : (parent.hovered ? "#34495e" : "#2c3e50"); radius: 4; border.color: parent.checked ? "#229954" : "#1a252f"; border.width: 1 }
                    contentItem: Text { text: parent.text; font.pointSize: 9; font.bold: true; color: parent.enabled ? "#ecf0f1" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: { topRoot.speedChanged(2.0) }
                }
            }

            Rectangle { width: 2; Layout.fillHeight: true; Layout.topMargin: 8; Layout.bottomMargin: 8
                gradient: Gradient { GradientStop { position: 0.0; color: "transparent" } GradientStop { position: 0.5; color: "#34495e" } GradientStop { position: 1.0; color: "transparent" } }
            }

            
            Row { spacing: 8
                Text { text: "Camera:"; color: "#ecf0f1"; font.pointSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                Button { width: 70; height: 32; text: "Follow"; checkable: true; checked: false; focusPolicy: Qt.NoFocus
                    background: Rectangle { color: parent.checked ? "#3498db" : (parent.hovered ? "#34495e" : "#2c3e50"); radius: 4; border.color: parent.checked ? "#2980b9" : "#1a252f"; border.width: 1 }
                    contentItem: Text { text: parent.text; font.pointSize: 9; font.bold: true; color: "#ecf0f1"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onToggled: { if (typeof game !== 'undefined' && game.cameraFollowSelection) game.cameraFollowSelection(checked) }
                }
                Button { width: 80; height: 32; text: "Reset"; focusPolicy: Qt.NoFocus
                    background: Rectangle { color: parent.hovered ? "#34495e" : "#2c3e50"; radius: 4; border.color: "#1a252f"; border.width: 1 }
                    contentItem: Text { text: parent.text; font.pointSize: 9; font.bold: true; color: "#ecf0f1"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: { if (typeof game !== 'undefined' && game.resetCamera) game.resetCamera() }
                }
            }

            Rectangle { width: 2; Layout.fillHeight: true; Layout.topMargin: 8; Layout.bottomMargin: 8
                gradient: Gradient { GradientStop { position: 0.0; color: "transparent" } GradientStop { position: 0.5; color: "#34495e" } GradientStop { position: 1.0; color: "transparent" } }
            }

            
            Text { text: "🗡️ " + (typeof game !== 'undefined' ? game.playerTroopCount : 0) + " / " + (typeof game !== 'undefined' ? game.maxTroopsPerPlayer : 0)
                color: { if (typeof game === 'undefined') return "#95a5a6"; var count = game.playerTroopCount; var max = game.maxTroopsPerPlayer; if (count >= max) return "#e74c3c"; if (count >= max * 0.8) return "#f39c12"; return "#2ecc71" }
                font.pointSize: 11; font.bold: true }

            Item { Layout.fillWidth: true }

            
            Rectangle { Layout.preferredWidth: Math.min(140, parent.width * 0.12); Layout.fillHeight: true; Layout.topMargin: 4; Layout.bottomMargin: 4; color: "#1a252f"; border.width: 2; border.color: "#3498db"; radius: 4
                Rectangle { anchors.fill: parent; anchors.margins: 2; color: "#0a0f14"
                    Text { anchors.centerIn: parent; text: "MINIMAP"; color: "#34495e"; font.pointSize: 9; font.bold: true }
                }
            }
        }
    }
}