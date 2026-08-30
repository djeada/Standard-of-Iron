import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: spectatorRoot

    property int selection_tick: 0
    property int refresh_tick: 0
    property var engine: (typeof game !== 'undefined') ? game : null

    signal follow_requested(int owner_id)

    function game_ready() {
        return spectatorRoot.engine !== null && spectatorRoot.engine !== undefined;
    }

    function followed_owner_id() {
        if (!spectatorRoot.game_ready())
            return -1;
        return spectatorRoot.engine.selected_player_id > 0 ? spectatorRoot.engine.selected_player_id : -1;
    }

    function contenders() {
        if (!spectatorRoot.game_ready())
            return [];
        var owners = spectatorRoot.engine.owner_info;
        var out = [];
        for (var i = 0; i < owners.length; ++i) {
            var owner = owners[i];
            if (owner.type !== "Player" && owner.type !== "AI")
                continue;
            if (owner.is_contender === false)
                continue;
            var stats = spectatorRoot.engine.get_player_stats(owner.id);
            var state = owner.state !== undefined ? owner.state : {};
            out.push({
                    "ownerId": owner.id,
                    "name": owner.name,
                    "accent": owner.color,
                    "teamId": owner.team_id,
                    "factionId": owner.nation ? owner.nation : "",
                    "factionName": owner.nation ? Design.FactionTheme.nameFor(owner.nation) : "",
                    "manpower": state.manpower !== undefined ? state.manpower : 0,
                    "manpowerCap": state.manpower_cap !== undefined ? state.manpower_cap : 0,
                    "kills": stats.enemiesKilled,
                    "losses": stats.losses,
                    "trained": stats.troopsRecruited,
                    "holdings": stats.barracksOwned
                });
        }
        out.sort(function (a, b) {
                if (a.teamId !== b.teamId)
                    return a.teamId - b.teamId;
                return a.ownerId - b.ownerId;
            });
        return out;
    }

    property var board: []

    function refresh() {
        spectatorRoot.board = spectatorRoot.contenders();
    }

    function follow_next(step) {
        var armies = spectatorRoot.board;
        if (armies.length === 0)
            return;
        var current = spectatorRoot.followed_owner_id();
        var index = 0;
        for (var i = 0; i < armies.length; ++i) {
            if (armies[i].ownerId === current) {
                index = i;
                break;
            }
        }
        var next = armies[(index + step + armies.length) % armies.length];
        spectatorRoot.follow_requested(next.ownerId);
    }

    onSelection_tickChanged: spectatorRoot.refresh()
    onRefresh_tickChanged: spectatorRoot.refresh()
    Component.onCompleted: spectatorRoot.refresh()

    Timer {
        interval: 500
        repeat: true
        running: spectatorRoot.visible
        onTriggered: spectatorRoot.refresh()
    }

    Design.IronPanel {
        anchors.fill: parent
        anchors.margins: Design.Metrics.space8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Design.Metrics.space12
            spacing: Design.Metrics.space8

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space12

                Text {
                    objectName: "spectatorBoardTitle"
                    text: qsTr("Armies of the field")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.subheading
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    objectName: "spectatorFollowLabel"
                    text: qsTr("Following: %1").arg(spectatorRoot.followed_name())
                    color: Design.Theme.textSecondary
                    font.pixelSize: Design.Typography.body
                }

                Design.IronButton {
                    objectName: "spectatorFollowPrev"
                    text: "\u25c0"
                    accessibleName: qsTr("Follow the previous army")
                    onClicked: spectatorRoot.follow_next(-1)
                }

                Design.IronButton {
                    objectName: "spectatorFollowNext"
                    text: "\u25b6"
                    accessibleName: qsTr("Follow the next army")
                    onClicked: spectatorRoot.follow_next(1)
                }
            }

            ListView {
                id: board_view

                objectName: "spectatorBoard"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Design.Metrics.space4
                model: spectatorRoot.board
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: Design.IronScrollBar {
                    objectName: "spectatorBoardScrollBar"
                }

                delegate: Rectangle {
                    objectName: "spectatorArmyRow"

                    property var army: modelData
                    readonly property bool followed: army.ownerId === spectatorRoot.followed_owner_id()

                    width: board_view.width
                    height: Design.A11y.scaled(34)
                    radius: Design.Metrics.radiusSmall
                    color: followed ? Design.Theme.backgroundRaised : "transparent"
                    border.width: followed ? Design.Metrics.borderThin : 0
                    border.color: army.accent

                    MouseArea {
                        anchors.fill: parent
                        onClicked: spectatorRoot.follow_requested(army.ownerId)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.rightMargin: Design.Metrics.space8
                        spacing: Design.Metrics.space12

                        Rectangle {
                            Layout.preferredWidth: Design.Metrics.space8
                            Layout.preferredHeight: parent.height - Design.Metrics.space8
                            radius: 2
                            color: army.accent
                        }

                        Text {
                            Layout.preferredWidth: Design.A11y.scaled(150)
                            text: army.name
                            elide: Text.ElideRight
                            color: Design.Theme.textPrimary
                            font.pixelSize: Design.Typography.body
                        }

                        Text {
                            Layout.preferredWidth: Design.A11y.scaled(130)
                            text: army.factionName
                            elide: Text.ElideRight
                            color: Design.Theme.textDisabled
                            font.pixelSize: Design.Typography.caption
                        }

                        Text {
                            objectName: "spectatorArmyManpower"
                            Layout.preferredWidth: Design.A11y.scaled(120)
                            text: qsTr("Manpower %1/%2").arg(army.manpower).arg(army.manpowerCap)
                            color: Design.Theme.textSecondary
                            font.pixelSize: Design.Typography.caption
                        }

                        Text {
                            Layout.preferredWidth: Design.A11y.scaled(100)
                            text: qsTr("Holdings %1").arg(army.holdings)
                            color: Design.Theme.textSecondary
                            font.pixelSize: Design.Typography.caption
                        }

                        Text {
                            Layout.preferredWidth: Design.A11y.scaled(110)
                            text: qsTr("Trained %1").arg(army.trained)
                            color: Design.Theme.textSecondary
                            font.pixelSize: Design.Typography.caption
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Kills %1  Losses %2").arg(army.kills).arg(army.losses)
                            color: Design.Theme.textSecondary
                            font.pixelSize: Design.Typography.caption
                        }
                    }
                }
            }
        }
    }

    function followed_name() {
        var current = spectatorRoot.followed_owner_id();
        for (var i = 0; i < spectatorRoot.board.length; ++i) {
            if (spectatorRoot.board[i].ownerId === current)
                return spectatorRoot.board[i].name;
        }
        return qsTr("the whole field");
    }
}
