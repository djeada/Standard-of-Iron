import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "BattleSummary"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    function fakeEngine(victoryState, owners, stats, missionTitle) {
        return engineComponent.createObject(testCase, {
                "victory_state": victoryState,
                "owner_info": owners,
                "stats_by_owner": stats,
                "mission_title": missionTitle !== undefined ? missionTitle : ""
            });
    }

    function owner(id, name, teamId, type, isLocal, nation, color) {
        return {
            "id": id,
            "name": name,
            "team_id": teamId,
            "type": type,
            "isLocal": isLocal,
            "nation": nation,
            "color": color
        };
    }

    function stats(kills, losses, trained, barracks, playTime) {
        return {
            "enemiesKilled": kills,
            "losses": losses,
            "troopsRecruited": trained,
            "barracksOwned": barracks,
            "playTimeSec": playTime,
            "gameEnded": true
        };
    }

    function standardMatch(victoryState, missionTitle) {
        var owners = [testCase.owner(1, "Scipio", 0, "Player", true, "roman_republic", "#c9a227"), testCase.owner(2, "Hasdrubal", 1, "AI", false, "carthage", "#9b59b6"), testCase.owner(3, "Wildlife", 9, "Neutral", false, "", "#888888")];
        var byOwner = {
            "1": testCase.stats(148, 62, 96, 7, 1834),
            "2": testCase.stats(62, 148, 71, 2, 1834),
            "3": testCase.stats(0, 0, 0, 0, 0)
        };
        return testCase.fakeEngine(victoryState, owners, byOwner, missionTitle);
    }

    function makeSummary(engine) {
        var host = hostComponent.createObject(testCase, {
                "width": 1280,
                "height": 720
            });
        host.summary.engine = engine;
        host.summary.show();
        wait(1);
        return host;
    }

    function test_an_ambient_faction_is_not_listed_as_an_army() {
        var owners = [testCase.owner(1, "Scipio", 0, "Player", true, "roman_republic", "#c9a227"), testCase.owner(2, "Hasdrubal", 1, "AI", false, "carthage", "#9b59b6"), testCase.owner(99, "Iron Sepulcher tomb_1", 9, "AI", false, "iron_sepulcher", "#4b3f6b")];
        owners[2].is_contender = false;
        var byOwner = {
            "1": testCase.stats(148, 62, 96, 7, 1834),
            "2": testCase.stats(62, 148, 71, 2, 1834),
            "99": testCase.stats(0, 0, 0, 4, 1834)
        };
        var engine = testCase.fakeEngine("victory", owners, byOwner, "");
        var host = testCase.makeSummary(engine);
        compare(host.summary.armies.length, 2, "an ambient faction was listed as an army");
        for (var i = 0; i < host.summary.armies.length; ++i)
            verify(host.summary.armies[i].ownerId !== 99, "the tomb owner reached the roster");
        engine.destroy();
        host.destroy();
    }

    function test_a_spectated_match_crowns_the_side_still_holding_ground() {
        var owners = [testCase.owner(1, "CPU I", 1, "AI", false, "roman_republic", "#c9a227"), testCase.owner(2, "CPU II", 2, "AI", false, "carthage", "#9b59b6")];
        var byOwner = {
            "1": testCase.stats(120, 40, 60, 3, 900),
            "2": testCase.stats(40, 120, 55, 0, 900)
        };
        var engine = testCase.fakeEngine("spectator", owners, byOwner, "");
        var host = testCase.makeSummary(engine);
        compare(host.summary.is_spectator, true);
        compare(host.summary.outcome, "spectator");
        for (var i = 0; i < host.summary.armies.length; ++i) {
            var army = host.summary.armies[i];
            compare(army.isWinner, army.ownerId === 1, army.name + " was given the wrong verdict");
        }
        engine.destroy();
        host.destroy();
    }

    function test_only_commanders_are_listed() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        compare(host.summary.armies.length, 2, "a neutral owner was listed as an army");
        engine.destroy();
        host.destroy();
    }

    function test_the_roster_is_ranked_by_score() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        compare(host.summary.armies[0].name, "Scipio");
        verify(host.summary.armies[0].score > host.summary.armies[1].score);
        engine.destroy();
        host.destroy();
    }

    function test_the_local_commander_is_marked_and_coloured() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        var local = host.summary.armies[0];
        compare(local.isLocal, true);
        compare(String(local.accent), "#c9a227");
        compare(local.factionName, FactionTheme.nameFor("roman_republic"));
        engine.destroy();
        host.destroy();
    }

    function test_a_win_crowns_the_local_team() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        compare(host.summary.armies[0].isWinner, true);
        compare(host.summary.armies[1].isWinner, false);
        engine.destroy();
        host.destroy();
    }

    function test_a_loss_crowns_the_other_team() {
        var engine = testCase.standardMatch("defeat");
        var host = testCase.makeSummary(engine);
        for (var i = 0; i < host.summary.armies.length; ++i) {
            var army = host.summary.armies[i];
            compare(army.isWinner, !army.isLocal, army.name + " was given the wrong verdict");
        }
        engine.destroy();
        host.destroy();
    }

    function test_the_report_reads_the_longest_watch_as_the_duration() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        compare(host.summary.durationText, Numerals.span(1834));
        engine.destroy();
        host.destroy();
    }

    function test_a_skirmish_carries_no_mission_name() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        compare(host.summary.missionName, "");
        engine.destroy();
        host.destroy();
    }

    function test_a_campaign_mission_names_itself() {
        var engine = testCase.standardMatch("victory", "The Plains of Cannae");
        var host = testCase.makeSummary(engine);
        compare(host.summary.missionName, "The Plains of Cannae");
        engine.destroy();
        host.destroy();
    }

    function test_the_report_survives_a_match_it_cannot_read() {
        var host = testCase.makeSummary(null);
        compare(host.summary.armies.length, 0);
        verify(findChild(host.summary, "battleReport") !== null);
        host.destroy();
    }

    function test_closing_the_report_calls_back() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        var closed = 0;
        host.summary.closed.connect(function () {
                closed += 1;
            });
        var report = findChild(host.summary, "battleReport");
        report.dismissed();
        compare(closed, 1);
        compare(host.summary.visible, false);
        engine.destroy();
        host.destroy();
    }

    function test_asking_for_the_menu_calls_back() {
        var engine = testCase.standardMatch("victory");
        var host = testCase.makeSummary(engine);
        var asked = 0;
        host.summary.return_to_main_menu_requested.connect(function () {
                asked += 1;
            });
        var report = findChild(host.summary, "battleReport");
        report.menuRequested();
        compare(asked, 1);
        engine.destroy();
        host.destroy();
    }

    Component {
        id: engineComponent

        QtObject {
            property string victory_state: ""
            property var owner_info: []
            property var stats_by_owner: ({})
            property string mission_title: ""
            property QtObject setup: QtObject {
                readonly property bool is_mission_match: mission_title !== ""

                function current_mission_objectives() {
                    return {
                        "title": mission_title
                    };
                }
            }

            function get_player_stats(ownerId) {
                return stats_by_owner[String(ownerId)];
            }
        }
    }

    Component {
        id: hostComponent

        Item {
            property alias summary: battleSummary

            BattleSummary {
                id: battleSummary

                anchors.fill: parent
            }
        }
    }
}
