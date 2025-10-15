import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: victoryOverlay

    property bool showingSummary: false

    signal returnToMainMenuRequested()

    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.7)
    visible: (typeof game !== 'undefined' && game.victoryState !== "")
    z: 100

    Rectangle {
        id: initialOverlay

        anchors.fill: parent
        color: "transparent"
        visible: !showingSummary

        Column {
            anchors.centerIn: parent
            spacing: 20

            Text {
                id: victoryText

                anchors.horizontalCenter: parent.horizontalCenter
                text: (typeof game !== 'undefined' && game.victoryState === "victory") ? "VICTORY!" : "DEFEAT"
                color: (typeof game !== 'undefined' && game.victoryState === "victory") ? "#27ae60" : "#e74c3c"
                font.pointSize: 48
                font.bold: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: (typeof game !== 'undefined' && game.victoryState === "victory") ? "Enemy barracks destroyed!" : "Your barracks was destroyed!"
                color: "white"
                font.pointSize: 18
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Continue"
                font.pointSize: 14
                focusPolicy: Qt.NoFocus
                onClicked: {
                    showingSummary = true;
                    battleSummary.show();
                }
            }

        }

    }

    BattleSummary {
        id: battleSummary

        anchors.fill: parent
        visible: showingSummary
        onClose: function() {
            showingSummary = false;
        }
        onReturnToMainMenu: function() {
            victoryOverlay.returnToMainMenuRequested();
        }
    }

}
