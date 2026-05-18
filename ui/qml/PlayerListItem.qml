import QtQuick 2.15
import QtQuick.Controls 2.15
import "ui_audio.js" as UiAudio

Rectangle {
    id: root

    property var colors: ({})
    property var player_data: ({})
    property var team_icons: []
    property bool can_remove: true

    signal remove_clicked
    signal color_clicked
    signal team_clicked
    signal faction_clicked

    width: parent ? parent.width : 400
    height: 48
    radius: 6
    color: colors.cardBaseB
    border.color: colors.thumbBr
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 10

        Item {
            width: 80
            height: parent.height

            Text {
                anchors.centerIn: parent
                text: player_data.playerName || ""
                color: player_data.isHuman ? colors.addColor : colors.textMain
                font.pixelSize: 14
                font.bold: player_data.isHuman || false
            }
        }

        Rectangle {
            width: 90
            height: parent.height
            radius: 4
            color: player_data.colorHex || "#666666"
            border.color: Qt.lighter(player_data.colorHex || "#666666", 1.3)
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: player_data.colorName || "Color"
                color: "white"
                font.pixelSize: 11
                font.bold: true
                style: Text.Outline
                styleColor: "black"
            }

            MouseArea {
                id: colorMouseArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onContainsMouseChanged: {
                    if (containsMouse && typeof game !== "undefined")
                        UiAudio.play_hover(game.audio_system);
                }
                onClicked: {
                    if (typeof game !== "undefined")
                        UiAudio.play_click(game.audio_system);
                    root.color_clicked();
                }
            }

            ToolTip {
                visible: colorMouseArea.containsMouse
                text: "Click to change color"
                delay: 500
            }
        }

        Rectangle {
            width: 50
            height: parent.height
            radius: 4
            color: colors.hoverBg
            border.color: colors.thumbBr
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: {
                        if (!player_data.team_id || !team_icons || team_icons.length === 0)
                            return "●";
                        return team_icons[(player_data.team_id - 1) % team_icons.length];
                    }
                    color: colors.textMain
                    font.pixelSize: 18
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "T" + (player_data.team_id || 1)
                    color: colors.textSubLite
                    font.pixelSize: 9
                }
            }

            MouseArea {
                id: teamMouseArea

                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onContainsMouseChanged: {
                    if (containsMouse && typeof game !== "undefined")
                        UiAudio.play_hover(game.audio_system);
                }
                onClicked: {
                    if (typeof game !== "undefined")
                        UiAudio.play_click(game.audio_system);
                    root.team_clicked();
                }
            }

            ToolTip {
                visible: teamMouseArea.containsMouse
                text: "Click to change team"
                delay: 500
            }
        }

        Rectangle {
            width: 140
            height: parent.height
            radius: 4
            color: colors.cardBaseA
            border.color: colors.thumbBr
            border.width: 1
            opacity: 0.7

            Text {
                anchors.centerIn: parent
                text: player_data.factionName || "Standard of Iron"
                color: colors.textSub
                font.pixelSize: 11
                elide: Text.ElideRight
                width: parent.width - 8
                horizontalAlignment: Text.AlignHCenter
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.ArrowCursor
                enabled: false
                onClicked: root.faction_clicked()
            }
        }

        Item {
            width: Math.max(10, parent.parent.width - 432)
            height: parent.height
        }

        Rectangle {
            width: 32
            height: parent.height
            radius: 4
            color: removeMouseArea.containsMouse ? colors.dangerColor : colors.cardBaseA
            border.color: colors.dangerColor
            border.width: 1
            visible: root.can_remove && !player_data.isHuman

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: "white"
                font.pixelSize: 16
                font.bold: true
            }

            MouseArea {
                id: removeMouseArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onContainsMouseChanged: {
                    if (containsMouse && typeof game !== "undefined")
                        UiAudio.play_hover(game.audio_system);
                }
                onClicked: {
                    if (typeof game !== "undefined")
                        UiAudio.play_click(game.audio_system);
                    root.remove_clicked();
                }
            }

            ToolTip {
                visible: removeMouseArea.containsMouse
                text: "Remove player"
                delay: 300
            }

            Behavior on color  {
                ColorAnimation {
                    duration: 150
                }
            }
        }
    }
}
