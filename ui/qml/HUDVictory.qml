import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: victoryOverlay

    property bool showingSummary: false
    property bool manuallyHidden: false

    signal returnToMainMenuRequested()

    function resetState() {
        showingSummary = false;
        manuallyHidden = false;
    }

    function forceHide() {
        resetState();
        manuallyHidden = true;
    }

    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.7)
    visible: !manuallyHidden && (typeof game !== 'undefined' && game.victoryState !== "")
    z: 100
    onVisibleChanged: {
        if (!visible)
            resetState();

    }

    Connections {
        function onVictoryStateChanged() {
            if (typeof game !== 'undefined' && game.victoryState === "")
                resetState();
            else if (typeof game !== 'undefined' && game.victoryState !== "")
                manuallyHidden = false;
        }

        target: (typeof game !== 'undefined') ? game : null
    }

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
            resetState();
            victoryOverlay.returnToMainMenuRequested();
        }
    }

}
