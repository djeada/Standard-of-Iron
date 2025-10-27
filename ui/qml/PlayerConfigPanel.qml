import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property var colors: ({
    })
    property var playersModel: null
    property var teamIcons: []
    property var currentMapData: null
    property string mapTitle: "Select a map"
    property string mapPreview: ""

    signal addCPUClicked()
    signal removePlayerClicked(int index)
    signal playerColorClicked(int index)
    signal playerTeamClicked(int index)
    signal playerFactionClicked(int index)

    anchors.fill: parent

    Component {
        id: playerListItemComponent

        Loader {
            property var itemColors: root.colors
            property var itemPlayerData: model
            property var itemTeamIcons: root.teamIcons
            property bool itemCanRemove: !model.isHuman

            source: "./PlayerListItem.qml"
            onLoaded: {
                item.colors = Qt.binding(function() {
                    return itemColors;
                });
                item.playerData = Qt.binding(function() {
                    return itemPlayerData;
                });
                item.teamIcons = Qt.binding(function() {
                    return itemTeamIcons;
                });
                item.canRemove = Qt.binding(function() {
                    return itemCanRemove;
                });
                item.removeClicked.connect(function() {
                    root.removePlayerClicked(index);
                });
                item.colorClicked.connect(function() {
                    root.playerColorClicked(index);
                });
                item.teamClicked.connect(function() {
                    root.playerTeamClicked(index);
                });
                item.factionClicked.connect(function() {
                    root.playerFactionClicked(index);
                });
            }
        }

    }

    Text {
        id: title

        text: root.mapTitle
        color: colors.textMain
        font.pixelSize: 20
        font.bold: true
        elide: Text.ElideRight

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

    }

    Item {
        id: playerSection

        height: Math.min(350, parent.height * 0.6)

        anchors {
            top: title.bottom
            topMargin: 16
            left: parent.left
            right: parent.right
        }

        Text {
            id: playerSectionTitle

            text: "Players (" + (playersModel ? playersModel.count : 0) + ")"
            color: colors.textMain
            font.pixelSize: 16
            font.bold: true
        }

        Text {
            id: playerSectionHint

            text: "Click color/team to cycle"
            color: colors.textSubLite
            font.pixelSize: 11
            font.italic: true

            anchors {
                left: playerSectionTitle.right
                leftMargin: 10
                verticalCenter: playerSectionTitle.verticalCenter
            }

        }

        Rectangle {
            id: playerListFrame

            radius: 8
            color: colors.cardBaseA
            border.color: colors.panelBr
            border.width: 1
            clip: true

            anchors {
                top: playerSectionTitle.bottom
                topMargin: 10
                left: parent.left
                right: parent.right
                bottom: addCPUBtn.top
                bottomMargin: 8
            }

            ListView {
                id: playerListView

                anchors.fill: parent
                anchors.margins: 8
                model: root.playersModel
                spacing: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                delegate: playerListItemComponent

                Item {
                    anchors.fill: parent
                    visible: !playersModel || playersModel.count === 0

                    Text {
                        anchors.centerIn: parent
                        text: "Select a map to configure players"
                        color: colors.textSub
                        font.pixelSize: 13
                    }

                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

            }

        }

        Button {
            id: addCPUBtn

            text: "+ Add CPU"
            enabled: {
                if (!currentMapData || !currentMapData.player_ids)
                    return false;

                if (!playersModel)
                    return false;

                return playersModel.count < currentMapData.player_ids.length;
            }
            hoverEnabled: true
            onClicked: root.addCPUClicked()

            anchors {
                bottom: parent.bottom
                left: parent.left
            }

            MouseArea {
                id: addHover

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            }

            ToolTip {
                visible: addHover.containsMouse && enabled
                text: "Add AI player to the game"
                delay: 500
            }

            contentItem: Text {
                text: addCPUBtn.text
                font.pixelSize: 12
                font.bold: true
                color: enabled ? colors.addColor : colors.textSub
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 32
                radius: 6
                color: enabled ? (addCPUBtn.down ? Qt.darker(colors.addColor, 1.3) : (addHover.containsMouse ? Qt.darker(colors.addColor, 1.1) : colors.cardBaseA)) : colors.cardBaseA
                border.width: 1
                border.color: enabled ? colors.addColor : colors.thumbBr

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }

                }

            }

        }

        Text {
            text: {
                if (!currentMapData || !currentMapData.player_ids)
                    return "";

                if (!playersModel)
                    return "";

                var available = currentMapData.player_ids.length - playersModel.count;
                if (available <= 0)
                    return "Max players reached";

                return available + " slot" + (available > 1 ? "s" : "") + " available";
            }
            color: colors.textSubLite
            font.pixelSize: 11

            anchors {
                left: addCPUBtn.right
                leftMargin: 10
                verticalCenter: addCPUBtn.verticalCenter
            }

        }

    }

    Rectangle {
        id: preview

        radius: 8
        color: "#031314"
        border.color: colors.thumbBr
        border.width: 1
        clip: true

        anchors {
            top: playerSection.bottom
            topMargin: 16
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        Image {
            id: previewImage

            anchors.fill: parent
            source: root.mapPreview
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready
        }

        Text {
            anchors.centerIn: parent
            visible: !previewImage.visible
            text: "(map preview)"
            color: colors.hint
            font.pixelSize: 14
        }

    }

}
