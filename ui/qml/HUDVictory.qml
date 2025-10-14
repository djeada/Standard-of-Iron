import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: victoryOverlay

    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.7)
    visible: (typeof game !== 'undefined' && game.victoryState !== "")
    z: 100

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
                victoryOverlay.visible = false;
                battleSummary.show();
            }
        }

    }

    BattleSummary {
        id: battleSummary

        anchors.fill: parent
    }

}
