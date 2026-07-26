import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: summaryOverlay

    property bool is_victory: (typeof game !== 'undefined' && game.victory_state === "victory")
    property var on_close: null
    property var on_return_to_main_menu: null

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

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

    function winning_team_id(owners, localTeamId) {
        if (is_victory)
            return localTeamId;
        for (var i = 0; i < owners.length; ++i) {
            if (owners[i].team_id !== localTeamId && (owners[i].type === "Player" || owners[i].type === "AI"))
                return owners[i].team_id;
        }
        return -1;
    }

    function build_player_list() {
        playerBannersModel.clear();
        if (!game_ready())
            return;
        var owners = game.owner_info;
        var localOwnerId = -1;
        var localTeamId = -1;
        for (var i = 0; i < owners.length; ++i) {
            if (owners[i].isLocal) {
                localOwnerId = owners[i].id;
                localTeamId = owners[i].team_id;
                break;
            }
        }
        var winningTeam = winning_team_id(owners, localTeamId);
        var banners = [];
        for (var j = 0; j < owners.length; ++j) {
            var owner = owners[j];
            if (owner.type !== "Player" && owner.type !== "AI")
                continue;
            var stats = game.get_player_stats(owner.id);
            banners.push({
                    "owner_id": owner.id,
                    "name": owner.name,
                    "isLocalPlayer": owner.id === localOwnerId,
                    "isWinner": owner.team_id === winningTeam,
                    "kills": stats.enemiesKilled,
                    "losses": stats.losses,
                    "unitsTrained": stats.troopsRecruited,
                    "villages": stats.barracksOwned,
                    "playTime": format_play_time(stats.playTimeSec),
                    "score": calculate_score(stats)
                });
        }
        banners.sort(function (a, b) {
                return (b.isLocalPlayer ? 1 : 0) - (a.isLocalPlayer ? 1 : 0);
            });
        for (var k = 0; k < banners.length; ++k)
            playerBannersModel.append(banners[k]);
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
    visible: false
    z: 101

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        opacity: 0.94
    }

    Column {
        anchors.centerIn: parent
        spacing: Design.Metrics.space24
        width: Math.min(parent.width * 0.95, Design.Metrics.space24 * 58)

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: summaryOverlay.is_victory ? qsTr("Victory Secured") : qsTr("Army Broken")
            color: summaryOverlay.is_victory ? Design.Theme.success : Design.Theme.danger
            font.family: Design.Typography.displayFamily
            font.pixelSize: Design.Typography.hero
            font.weight: Design.Typography.bold
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Design.Metrics.space16

            Repeater {
                model: playerBannersModel

                delegate: Design.IronPanel {
                    id: banner

                    required property var model

                    width: Design.Metrics.space24 * 10
                    height: bannerColumn.implicitHeight + Design.Metrics.space24
                    raised: banner.model.isLocalPlayer

                    border.color: banner.model.isWinner ? Design.Theme.success : Design.Theme.danger
                    border.width: banner.model.isLocalPlayer ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                    accessibleName: banner.model.name

                    Column {
                        id: bannerColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: Design.Metrics.space8

                        Design.IronBadge {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: banner.model.isLocalPlayer
                            text: qsTr("YOU")
                        }

                        Text {
                            width: parent.width
                            text: banner.model.name
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.displayFamily
                            font.pixelSize: Design.Typography.heading
                            font.weight: Design.Typography.bold
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }

                        Design.IronBadge {
                            anchors.horizontalCenter: parent.horizontalCenter
                            tone: banner.model.isWinner ? Design.Theme.success : Design.Theme.danger
                            text: banner.model.isWinner ? qsTr("Held the Field") : qsTr("Routed")
                        }

                        Design.IronDivider {
                            width: parent.width
                        }

                        Repeater {
                            model: [{
                                    "label": qsTr("Kills"),
                                    "value": banner.model.kills
                                }, {
                                    "label": qsTr("Losses"),
                                    "value": banner.model.losses
                                }, {
                                    "label": qsTr("Units trained"),
                                    "value": banner.model.unitsTrained
                                }, {
                                    "label": qsTr("Villages"),
                                    "value": banner.model.villages
                                }, {
                                    "label": qsTr("Play time"),
                                    "value": banner.model.playTime
                                }]

                            delegate: Item {
                                required property var modelData

                                width: bannerColumn.width
                                height: statLabel.implicitHeight

                                Text {
                                    id: statLabel

                                    anchors.left: parent.left
                                    text: modelData.label
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                }

                                Text {
                                    anchors.right: parent.right
                                    text: modelData.value
                                    color: Design.Theme.textPrimary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    font.weight: Design.Typography.medium
                                }
                            }
                        }

                        Design.IronDivider {
                            width: parent.width
                        }

                        Text {
                            width: parent.width
                            text: qsTr("SCORE")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            width: parent.width
                            text: banner.model.score
                            color: Design.Theme.accent
                            font.family: Design.Typography.displayFamily
                            font.pixelSize: Design.Typography.title
                            font.weight: Design.Typography.bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Design.Metrics.space8

            Design.IronButton {
                text: qsTr("Return to Menu")
                tone: "primary"
                onClicked: summaryOverlay.return_to_main_menu()
            }

            Design.IronButton {
                text: qsTr("Back")
                onClicked: summaryOverlay.hide()
            }
        }
    }

    ListModel {
        id: playerBannersModel
    }
}
