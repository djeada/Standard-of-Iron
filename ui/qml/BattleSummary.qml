import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: summaryOverlay

    property var engine: (typeof game !== 'undefined') ? game : null
    property bool is_victory: summaryOverlay.engine !== null && summaryOverlay.engine.victory_state === "victory"
    property bool is_spectator: summaryOverlay.engine !== null && summaryOverlay.engine.victory_state === "spectator"
    property string outcome: summaryOverlay.is_spectator ? "spectator" : (summaryOverlay.is_victory ? "victory" : "defeat")
    property string headline: ""
    property string subtitle: ""
    property string factionId: ""
    property string missionName: ""
    property string durationText: ""
    property var armies: []
    property bool prepared: false
    property bool preparing: false
    property bool returning_to_menu: false
    property int preparation_generation: 0

    signal closed
    signal return_to_main_menu_requested

    function reset_data() {
        summaryOverlay.preparation_generation += 1;
        summaryOverlay.prepared = false;
        summaryOverlay.preparing = false;
        summaryOverlay.returning_to_menu = false;
        menuTransitionTimer.stop();
        summaryOverlay.armies = [];
        summaryOverlay.durationText = "";
        summaryOverlay.missionName = "";
    }

    function prepare() {
        if (summaryOverlay.prepared || summaryOverlay.preparing)
            return;
        summaryOverlay.preparing = true;
        var generation = summaryOverlay.preparation_generation;
        Qt.callLater(function () {
                if (generation !== summaryOverlay.preparation_generation)
                    return;
                summaryOverlay.build_army_list();
                if (generation !== summaryOverlay.preparation_generation)
                    return;
                summaryOverlay.prepared = true;
                summaryOverlay.preparing = false;
            });
    }

    function show() {
        summaryOverlay.returning_to_menu = false;
        menuTransitionTimer.stop();
        visible = true;
        report.forceActiveFocus();
        summaryOverlay.prepare();
    }

    function hide() {
        summaryOverlay.returning_to_menu = false;
        menuTransitionTimer.stop();
        visible = false;
        summaryOverlay.closed();
    }

    function return_to_main_menu() {
        if (summaryOverlay.returning_to_menu)
            return;
        summaryOverlay.returning_to_menu = true;
        menuTransitionTimer.restart();
    }

    function spectator_winning_team(roster) {
        var best = -1;
        var bestScore = -1;
        for (var i = 0; i < roster.length; ++i) {
            var entry = roster[i];
            var weight = entry.barracks * 1000000 + entry.score;
            if (weight > bestScore) {
                bestScore = weight;
                best = entry.teamId;
            }
        }
        return best;
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

    function calculate_score(stats) {
        return stats.enemiesKilled * 100 + stats.troopsRecruited * 10 + stats.barracksOwned * 500;
    }

    function build_army_list() {
        if (summaryOverlay.engine === null) {
            summaryOverlay.armies = [];
            return;
        }
        var owners = summaryOverlay.engine.owner_info;
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
        var roster = [];
        var longestPlayTime = 0;
        for (var j = 0; j < owners.length; ++j) {
            var owner = owners[j];
            if (owner.type !== "Player" && owner.type !== "AI")
                continue;
            if (owner.is_contender === false)
                continue;
            var stats = summaryOverlay.engine.get_player_stats(owner.id);
            longestPlayTime = Math.max(longestPlayTime, stats.playTimeSec);
            roster.push({
                    "ownerId": owner.id,
                    "teamId": owner.team_id,
                    "name": owner.name,
                    "accent": owner.color,
                    "factionId": owner.nation ? owner.nation : "",
                    "factionName": owner.nation ? Design.FactionTheme.nameFor(owner.nation) : "",
                    "isLocal": owner.id === localOwnerId,
                    "isWinner": owner.team_id === winningTeam,
                    "kills": stats.enemiesKilled,
                    "losses": stats.losses,
                    "trained": stats.troopsRecruited,
                    "barracks": stats.barracksOwned,
                    "score": calculate_score(stats)
                });
        }
        if (summaryOverlay.is_spectator) {
            var spectatorWinner = spectator_winning_team(roster);
            for (var k = 0; k < roster.length; ++k)
                roster[k].isWinner = roster[k].teamId === spectatorWinner;
        }
        roster.sort(function (a, b) {
                return b.score - a.score;
            });
        summaryOverlay.armies = roster;
        summaryOverlay.durationText = Design.Numerals.span(longestPlayTime);
        summaryOverlay.missionName = current_mission_name();
    }

    function current_mission_name() {
        var setup = summaryOverlay.engine !== null ? summaryOverlay.engine.setup : null;
        if (!setup || !setup.is_mission_match || !setup.current_mission_objectives)
            return "";
        var objectives = setup.current_mission_objectives();
        return objectives && objectives.title ? objectives.title : "";
    }

    anchors.fill: parent
    visible: false
    z: 101

    Timer {
        id: menuTransitionTimer

        interval: 16
        repeat: false
        onTriggered: {
            if (summaryOverlay.returning_to_menu)
                summaryOverlay.return_to_main_menu_requested();
        }
    }

    Design.BattleReportLayout {
        id: report

        objectName: "battleReport"
        anchors.fill: parent
        enabled: !summaryOverlay.returning_to_menu
        outcome: summaryOverlay.outcome
        factionId: summaryOverlay.factionId
        headline: summaryOverlay.headline
        subtitle: summaryOverlay.subtitle
        missionName: summaryOverlay.missionName
        durationText: summaryOverlay.durationText
        armies: summaryOverlay.armies
        onDismissed: summaryOverlay.hide()
        onMenuRequested: summaryOverlay.return_to_main_menu()
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Design.Metrics.space24
        width: preparingLabel.implicitWidth + Design.Metrics.space24 * 2
        height: Design.Metrics.controlHeight
        radius: Design.Metrics.radiusSmall
        color: Design.Theme.panelIron
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderStrong
        visible: summaryOverlay.visible && summaryOverlay.preparing && !summaryOverlay.prepared && !summaryOverlay.returning_to_menu
        z: 102

        Text {
            id: preparingLabel

            anchors.centerIn: parent
            text: qsTr("Preparing battle report…")
            color: Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.label
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: summaryOverlay.visible && summaryOverlay.returning_to_menu
        z: 103
        color: Qt.rgba(Design.Theme.backgroundDeep.r, Design.Theme.backgroundDeep.g, Design.Theme.backgroundDeep.b, 0.72)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.centerIn: parent
            width: returningLabel.implicitWidth + Design.Metrics.space24 * 2
            height: Design.Metrics.controlHeight
            radius: Design.Metrics.radiusSmall
            color: Design.Theme.panelIron
            border.width: Design.Metrics.borderThin
            border.color: Design.Theme.accent

            Text {
                id: returningLabel

                anchors.centerIn: parent
                text: qsTr("Returning to main menu…")
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.medium
            }
        }
    }
}
