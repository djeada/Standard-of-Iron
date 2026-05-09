import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Rectangle {
    id: summaryOverlay

    property bool is_victory: (typeof game !== 'undefined' && game.victory_state === "victory")
    property var on_close: null
    property var on_return_to_main_menu: null
    readonly property var hs: StyleGuide.historical

    function show() {
        visible = true;
        build_player_list();
    }

    function hide() {
        visible = false;
        if (on_close)
            on_close();

    }

    function return_to_main_menu() {
        if (on_return_to_main_menu)
            on_return_to_main_menu();

    }

    function build_player_list() {
        playerBannersModel.clear();
        if (typeof game === 'undefined')
            return ;

        var owners = game.owner_info;
        var localOwnerId = -1;
        var localTeamId = -1;
        var winningTeamId = -1;
        for (var i = 0; i < owners.length; i++) {
            if (owners[i].isLocal) {
                localOwnerId = owners[i].id;
                localTeamId = owners[i].team_id;
                break;
            }
        }
        if (is_victory) {
            winningTeamId = localTeamId;
        } else {
            for (var t = 0; t < owners.length; t++) {
                if (owners[t].team_id !== localTeamId && (owners[t].type === "Player" || owners[t].type === "AI")) {
                    winningTeamId = owners[t].team_id;
                    break;
                }
            }
        }
        var playerBanners = [];
        var aiColorIndex = 1;
        for (var j = 0; j < owners.length; j++) {
            var owner = owners[j];
            if (owner.type === "Player" || owner.type === "AI") {
                var stats = game.get_player_stats(owner.id);
                var isLocalPlayer = (owner.id === localOwnerId);
                var isWinner = (owner.team_id === winningTeamId);
                var bannerColor = get_banner_color(owner.id, isLocalPlayer, owner.type === "AI", aiColorIndex);
                if (owner.type === "AI")
                    aiColorIndex++;

                var score = calculate_score(stats);
                var playTimeFormatted = format_play_time(stats.playTimeSec);
                playerBanners.push({
                    "owner_id": owner.id,
                    "name": owner.name,
                    "isLocalPlayer": isLocalPlayer,
                    "isAI": owner.type === "AI",
                    "isWinner": isWinner,
                    "bannerColor": bannerColor,
                    "kills": stats.enemiesKilled,
                    "losses": stats.losses,
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
        for (var k = 0; k < playerBanners.length; k++) playerBannersModel.append(playerBanners[k])
    }

    function get_banner_color(owner_id, isLocal, isAI, aiIndex) {
        var colors = ["#8F2F2A", "#496C4A", "#9A7A38", "#536D7A", "#6A5C7D", "#6F7F4B"];
        if (isLocal)
            return colors[0];

        if (isAI) {
            var idx = aiIndex % (colors.length - 1);
            return colors[idx + 1];
        }
        return colors[1];
    }

    function calculate_score(stats) {
        return stats.enemiesKilled * 100 + stats.troopsRecruited * 10 + stats.barracksOwned * 500;
    }

    function format_play_time(seconds) {
        var h = Math.floor(seconds / 3600);
        var m = Math.floor((seconds % 3600) / 60);
        var s = Math.floor(seconds % 60);
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
    }

    anchors.fill: parent
    color: Qt.rgba(8 / 255, 6 / 255, 4 / 255, 0.92)
    visible: false
    z: 101

    Column {
        anchors.centerIn: parent
        spacing: 30
        width: Math.min(parent.width * 0.95, 1400)

        Text {
            id: mainTitle

            anchors.horizontalCenter: parent.horizontalCenter
            text: is_victory ? qsTr("Victory Secured") : qsTr("Army Broken")
            color: is_victory ? Theme.accentBright : hs.waxHover
            font.family: "serif"
            font.pointSize: 52
            font.bold: true
            style: Text.Outline
            styleColor: hs.parchmentDark
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
                    height: 420
                    color: Qt.rgba(0, 0, 0, 0)

                    Rectangle {
                        id: dropShadow

                        anchors.fill: parent
                        anchors.topMargin: 8
                        anchors.leftMargin: 6
                        color: Qt.rgba(0, 0, 0, 0.5)
                        radius: 4
                        z: -1
                    }

                    Item {
                        anchors.fill: parent

                        Rectangle {
                            id: bannerTop

                            anchors.top: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width * 0.4
                            height: 20
                            color: hs.bronzeDeep
                            radius: 4

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                radius: 4
                                color: Theme.accentBright
                            }

                        }

                        Rectangle {
                            id: banner

                            anchors.top: bannerTop.bottom
                            anchors.topMargin: 5
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 10
                            height: parent.height - 30
                            color: model.bannerColor
                            radius: 4
                            border.color: hs.bronzeDeep
                            border.width: 4
                            opacity: model.isLocalPlayer ? 0.8 : 0.95

                            Rectangle {
                                visible: model.isLocalPlayer
                                anchors.fill: parent
                                color: Qt.rgba(0, 0, 0, 0.2)
                                radius: 4
                            }

                            Canvas {
                                id: fabricTexture

                                anchors.fill: parent
                                opacity: 0.15
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.strokeStyle = Qt.rgba(0, 0, 0, 0.3);
                                    ctx.lineWidth = 1;
                                    for (var y = 0; y < height; y += 4) {
                                        ctx.beginPath();
                                        ctx.moveTo(0, y);
                                        ctx.lineTo(width, y);
                                        ctx.stroke();
                                    }
                                    for (var x = 0; x < width; x += 4) {
                                        ctx.beginPath();
                                        ctx.moveTo(x, 0);
                                        ctx.lineTo(x, height);
                                        ctx.stroke();
                                    }
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 4
                                color: "transparent"
                                border.color: Theme.accentBright
                                border.width: 2
                                radius: 3
                            }

                            Column {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 10

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: model.name
                                    color: Theme.accentBright
                                    font.family: "serif"
                                    font.pointSize: 18
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    width: parent.width
                                    style: Text.Outline
                                    styleColor: hs.parchmentDark
                                }

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width * 0.8
                                    height: 24
                                    color: model.isWinner ? "#496C4A" : hs.waxDark
                                    radius: 4
                                    border.color: model.isWinner ? "#8FA46B" : hs.waxHover
                                    border.width: 2

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.isWinner ? qsTr("Held the Field") : qsTr("Routed")
                                        color: Theme.textMain
                                        font.family: "serif"
                                        font.pointSize: 11
                                        font.bold: true
                                        style: Text.Outline
                                        styleColor: hs.parchmentDark
                                    }

                                }

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width * 0.9
                                    height: 2
                                    color: hs.bronzeDeep
                                }

                                Column {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: qsTr("Kills: %1").arg(model.kills)
                                        color: Theme.textMain
                                        font.family: "serif"
                                        font.pointSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        style: Text.Outline
                                        styleColor: hs.parchmentDark
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: qsTr("Losses: %1").arg(model.losses)
                                        color: Theme.textMain
                                        font.family: "serif"
                                        font.pointSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        style: Text.Outline
                                        styleColor: hs.parchmentDark
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: qsTr("Units Trained: %1").arg(model.unitsTrained)
                                        color: Theme.textMain
                                        font.family: "serif"
                                        font.pointSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        style: Text.Outline
                                        styleColor: hs.parchmentDark
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: qsTr("Villages: %1").arg(model.villages)
                                        color: Theme.textMain
                                        font.family: "serif"
                                        font.pointSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        style: Text.Outline
                                        styleColor: hs.parchmentDark
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: qsTr("Play Time: %1").arg(model.playTime)
                                        color: Theme.textMain
                                        font.family: "serif"
                                        font.pointSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        style: Text.Outline
                                        styleColor: hs.parchmentDark
                                    }

                                }

                                Item {
                                    height: 8
                                }

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width * 0.9
                                    height: 2
                                    color: hs.bronzeDeep
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("SCORE")
                                    color: Theme.accentBright
                                    font.family: "serif"
                                    font.pointSize: 15
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    style: Text.Outline
                                    styleColor: hs.parchmentDark
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: model.score
                                    color: Theme.accentBright
                                    font.family: "serif"
                                    font.pointSize: 22
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    style: Text.Outline
                                    styleColor: hs.parchmentDark
                                }

                            }

                            Canvas {
                                id: tornEdge

                                visible: model.isLocalPlayer
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 25
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.fillStyle = model.bannerColor;
                                    ctx.beginPath();
                                    ctx.moveTo(0, 0);
                                    for (var i = 0; i <= width; i += 8) {
                                        var y = Math.random() * height;
                                        ctx.lineTo(i, y);
                                    }
                                    ctx.lineTo(width, height);
                                    ctx.lineTo(0, height);
                                    ctx.closePath();
                                    ctx.fill();
                                }
                            }

                        }

                        Rectangle {
                            id: tassel1

                            anchors.bottom: banner.bottom
                            anchors.bottomMargin: -15
                            anchors.left: banner.left
                            anchors.leftMargin: banner.width * 0.3
                            width: 3
                            height: 20
                            color: hs.bronzeDeep

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 8
                                height: 8
                                radius: 4
                                color: Theme.accentBright
                            }

                        }

                        Rectangle {
                            id: tassel2

                            anchors.bottom: banner.bottom
                            anchors.bottomMargin: -15
                            anchors.right: banner.right
                            anchors.rightMargin: banner.width * 0.3
                            width: 3
                            height: 20
                            color: hs.bronzeDeep

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 8
                                height: 8
                                radius: 4
                                color: Theme.accentBright
                            }

                        }

                    }

                    Rectangle {
                        visible: model.isLocalPlayer
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.topMargin: 25
                        anchors.leftMargin: -5
                        width: 80
                        height: 30
                        color: hs.bannerNeutral
                        radius: 4
                        border.color: Theme.accentBright
                        border.width: 2

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("YOU")
                            color: Theme.accentBright
                            font.family: "serif"
                            font.pointSize: 12
                            font.bold: true
                        }

                    }

                }

            }

        }

        StyledButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Return to Menu")
            focusPolicy: Qt.NoFocus
            onClicked: {
                summaryOverlay.return_to_main_menu();
            }
        }

    }

    ListModel {
        id: playerBannersModel
    }

}
