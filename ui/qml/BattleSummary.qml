import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: summaryOverlay

    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.85)
    visible: false
    z: 101

    property bool isVictory: (typeof game !== 'undefined' && game.victoryState === "victory")

    function show() {
        visible = true;
        buildPlayerList();
    }

    function hide() {
        visible = false;
    }

    function buildPlayerList() {
        playerBannersModel.clear();
        if (typeof game === 'undefined')
            return ;
        var owners = game.ownerInfo;
        var localOwnerId = -1;
        for (var i = 0; i < owners.length; i++) {
            if (owners[i].isLocal) {
                localOwnerId = owners[i].id;
                break;
            }
        }
        var playerBanners = [];
        for (var j = 0; j < owners.length; j++) {
            var owner = owners[j];
            if (owner.type === "Player" || owner.type === "AI") {
                var stats = game.getPlayerStats(owner.id);
                var isLocalPlayer = (owner.id === localOwnerId);
                var bannerColor = getBannerColor(owner.id, isLocalPlayer);
                var score = calculateScore(stats);
                var playTimeFormatted = formatPlayTime(stats.playTimeSec);
                playerBanners.push({
                    "ownerId": owner.id,
                    "name": owner.name,
                    "isLocalPlayer": isLocalPlayer,
                    "isAI": owner.type === "AI",
                    "bannerColor": bannerColor,
                    "kills": stats.enemiesKilled,
                    "losses": stats.troopsRecruited - (stats.barracksOwned > 0 ? 1 : 0),
                    "unitsTrained": stats.troopsRecruited,
                    "villages": stats.barracksOwned,
                    "playTime": playTimeFormatted,
                    "score": score
                });
            }
        }
        playerBanners.sort(function(a, b) {
            if (a.isLocalPlayer)
                return -1;
            if (b.isLocalPlayer)
                return 1;
            return 0;
        });
        for (var k = 0; k < playerBanners.length; k++)
            playerBannersModel.append(playerBanners[k]);
    }

    function getBannerColor(ownerId, isLocal) {
        var colors = ["#DC143C", "#228B22", "#FFD700", "#4169E1", "#9370DB", "#32CD32"];
        if (isLocal)
            return colors[0];
        var index = (ownerId - 1) % colors.length;
        return colors[index];
    }

    function calculateScore(stats) {
        return stats.enemiesKilled * 100 + stats.troopsRecruited * 10 + stats.barracksOwned * 500;
    }

    function formatPlayTime(seconds) {
        var h = Math.floor(seconds / 3600);
        var m = Math.floor((seconds % 3600) / 60);
        var s = Math.floor(seconds % 60);
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
    }

    Column {
        anchors.centerIn: parent
        spacing: 30
        width: Math.min(parent.width * 0.95, 1400)

        Text {
            id: mainTitle

            anchors.horizontalCenter: parent.horizontalCenter
            text: isVictory ? "VICTORY!" : "FAILURE!"
            color: "#FFD700"
            font.family: "serif"
            font.pointSize: 64
            font.bold: true
            style: Text.Outline
            styleColor: "#3a2a1a"
        }

        Row {
            id: bannersRow

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 20

            Repeater {
                model: playerBannersModel

                delegate: Rectangle {
                    id: bannerCard

                    width: 220
                    height: 400
                    color: Qt.rgba(0, 0, 0, 0)

                    Rectangle {
                        id: banner

                        anchors.fill: parent
                        anchors.margins: 5
                        color: model.bannerColor
                        radius: 8
                        border.color: "#8B7355"
                        border.width: 3
                        opacity: model.isLocalPlayer ? 0.75 : 0.95

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 3
                            color: "transparent"
                            border.color: "#FFD700"
                            border.width: 2
                            radius: 6
                        }

                        Rectangle {
                            visible: model.isLocalPlayer
                            anchors.fill: parent
                            color: Qt.rgba(0, 0, 0, 0.3)
                            radius: 8
                        }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 12

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: model.name
                                color: "#FFD700"
                                font.family: "serif"
                                font.pointSize: 18
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                width: parent.width
                                style: Text.Raised
                                styleColor: "#000000"
                            }

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width * 0.8
                                height: 2
                                color: "#8B7355"
                            }

                            Column {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 8
                                width: parent.width

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Kills: " + model.kills
                                    color: "#F5F5DC"
                                    font.family: "serif"
                                    font.pointSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Losses: " + model.losses
                                    color: "#F5F5DC"
                                    font.family: "serif"
                                    font.pointSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Units Trained: " + model.unitsTrained
                                    color: "#F5F5DC"
                                    font.family: "serif"
                                    font.pointSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Villages: " + model.villages
                                    color: "#F5F5DC"
                                    font.family: "serif"
                                    font.pointSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Play Time: " + model.playTime
                                    color: "#F5F5DC"
                                    font.family: "serif"
                                    font.pointSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                }

                            }

                            Item {
                                height: 10
                            }

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width * 0.8
                                height: 2
                                color: "#8B7355"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "SCORE"
                                color: "#FFD700"
                                font.family: "serif"
                                font.pointSize: 16
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                style: Text.Raised
                                styleColor: "#000000"
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: model.score
                                color: "#FFD700"
                                font.family: "serif"
                                font.pointSize: 24
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                style: Text.Raised
                                styleColor: "#000000"
                            }

                        }

                        Canvas {
                            id: tornEdge

                            visible: model.isLocalPlayer
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                ctx.strokeStyle = "#3a2a1a";
                                ctx.lineWidth = 2;
                                ctx.beginPath();
                                for (var i = 0; i < width; i += 10) {
                                    var y = height - 5 + Math.random() * 10;
                                    if (i === 0)
                                        ctx.moveTo(i, y);
                                    else
                                        ctx.lineTo(i, y);
                                }
                                ctx.stroke();
                            }
                        }

                    }

                    Rectangle {
                        visible: model.isLocalPlayer
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        anchors.leftMargin: -5
                        width: 80
                        height: 30
                        color: "#8B4513"
                        radius: 4
                        border.color: "#FFD700"
                        border.width: 2

                        Text {
                            anchors.centerIn: parent
                            text: "YOU"
                            color: "#FFD700"
                            font.family: "serif"
                            font.pointSize: 12
                            font.bold: true
                        }

                    }

                }

            }

        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Return to Menu"
            font.pointSize: 16
            font.bold: true
            padding: 12
            focusPolicy: Qt.NoFocus
            onClicked: {
                summaryOverlay.hide();
            }
        }

    }

    ListModel {
        id: playerBannersModel
    }

}
