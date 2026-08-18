import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: summaryOverlay

    property var engine: (typeof game !== 'undefined') ? game : null
    property bool is_victory: summaryOverlay.engine !== null && summaryOverlay.engine.victory_state === "victory"
    property string outcome: summaryOverlay.is_victory ? "victory" : "defeat"
    property string headline: ""
    property string subtitle: ""
    property string factionId: ""
    property string missionName: ""
    property string durationText: ""
    property var armies: []
    property var on_close: null
    property var on_return_to_main_menu: null

    function show() {
        visible = true;
        build_army_list();
        report.forceActiveFocus();
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
            var stats = summaryOverlay.engine.get_player_stats(owner.id);
            longestPlayTime = Math.max(longestPlayTime, stats.playTimeSec);
            roster.push({
                    "ownerId": owner.id,
                    "name": owner.name,
                    "accent": owner.color,
                    "factionId": owner.nation ? owner.nation : "",
                    "factionName": owner.nation ? Design.FactionTheme.nameFor(owner.nation) : "",
                    "isLocal": owner.id === localOwnerId,
                    "isWinner": owner.team_id === winningTeam,
                    "kills": stats.enemiesKilled,
                    "losses": stats.losses,
                    "trained": stats.troopsRecruited,
                    "villages": stats.barracksOwned,
                    "score": calculate_score(stats)
                });
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
        if (!setup || !setup.is_campaign_mission || !setup.current_mission_objectives)
            return "";
        var objectives = setup.current_mission_objectives();
        return objectives && objectives.title ? objectives.title : "";
    }

    anchors.fill: parent
    visible: false
    z: 101

    Design.BattleReportLayout {
        id: report

        objectName: "battleReport"
        anchors.fill: parent
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
}
