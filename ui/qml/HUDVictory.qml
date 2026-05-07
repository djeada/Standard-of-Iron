import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Rectangle {
    id: victoryOverlay

    property bool showingSummary: false
    property bool manuallyHidden: false
    readonly property var hs: StyleGuide.historical

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
    visible: !manuallyHidden && (typeof game !== 'undefined' && game.victory_state !== "")
    z: 100
    onVisibleChanged: {
        if (!visible)
            resetState();

    }

    Connections {
        function onVictory_state_changed() {
            if (typeof game !== 'undefined' && game.victory_state === "")
                resetState();
            else if (typeof game !== 'undefined' && game.victory_state !== "")
                manuallyHidden = false;
        }

        target: (typeof game !== 'undefined') ? game : null
    }

    Rectangle {
        id: initialOverlay

        anchors.fill: parent
        color: "transparent"
        visible: !showingSummary

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.7, 680)
            height: 260
            radius: Theme.radiusPanel
            color: hs.parchmentDark
            border.color: hs.bronze
            border.width: 2
            opacity: 0.96

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: Theme.radiusLarge
                color: hs.parchmentLight
                border.color: hs.bronzeDeep
                border.width: 1
                opacity: 0.7
            }

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: hs.romanGlyph + " · " + hs.carthageGlyph
                    color: Theme.accentBright
                    font.pointSize: 16
                    font.bold: true
                }

                Text {
                    id: victoryText

                    anchors.horizontalCenter: parent.horizontalCenter
                    text: (typeof game !== 'undefined' && game.victory_state === "victory") ? qsTr("VICTORY!") : qsTr("DEFEAT")
                    color: (typeof game !== 'undefined' && game.victory_state === "victory") ? Theme.successText : Theme.removeColor
                    font.pointSize: 48
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: (typeof game !== 'undefined' && game.victory_state === "victory") ? qsTr("Enemy barracks destroyed!") : qsTr("Your army was crushed")
                    color: Theme.textMain
                    font.pointSize: 18
                }

                StyledButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Continue")
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        showingSummary = true;
                        battleSummary.show();
                    }
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
