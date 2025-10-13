
import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: 48
    radius: 6

    property var colors: ({})
    property var playerData: ({})
    property var teamIcons: []
    property bool canRemove: true

    signal removeClicked()
    signal colorClicked()
    signal teamClicked()
    signal factionClicked()

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
                text: playerData.playerName || ""
                color: playerData.isHuman ? colors.addColor : colors.textMain
                font.pixelSize: 14
                font.bold: playerData.isHuman || false
            }
        }

        
        Rectangle {
            width: 90
            height: parent.height
            radius: 4
            color: playerData.colorHex || "#666666"
            border.color: Qt.lighter(playerData.colorHex || "#666666", 1.3)
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: playerData.colorName || "Color"
                color: "white"
                font.pixelSize: 11
                font.bold: true
                style: Text.Outline
                styleColor: "black"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.colorClicked()
            }

            ToolTip {
                visible: parent.children[1].containsMouse
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
                        if (!playerData.teamId || !teamIcons || teamIcons.length === 0) return "●"
                        return teamIcons[(playerData.teamId - 1) % teamIcons.length]
                    }
                    color: colors.textMain
                    font.pixelSize: 18
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "T" + (playerData.teamId || 1)
                    color: colors.textSubLite
                    font.pixelSize: 9
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: root.teamClicked()
            }

            ToolTip {
                visible: parent.children[1].containsMouse
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
                text: playerData.factionName || "Standard of Iron"
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
                onClicked: root.factionClicked()
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
            visible: root.canRemove && !playerData.isHuman
            Behavior on color { ColorAnimation { duration: 150 } }

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
                onClicked: root.removeClicked()
            }

            ToolTip {
                visible: removeMouseArea.containsMouse
                text: "Remove player"
                delay: 300
            }
        }
    }
}
