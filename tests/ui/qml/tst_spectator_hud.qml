import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "SpectatorHud"
    when: windowShown
    width: 1280
    height: 400
    visible: true

    function owner(id, name, teamId, nation, contender) {
        return {
            "id": id,
            "name": name,
            "team_id": teamId,
            "type": "AI",
            "isLocal": false,
            "nation": nation,
            "color": "#c9a227",
            "is_contender": contender,
            "state": {
                "manpower": id * 10,
                "manpower_cap": 280
            }
        };
    }

    function makeHud(engine) {
        var host = hostComponent.createObject(testCase, {
                "width": 1280,
                "height": 360
            });
        verify(host !== null, "the spectator host was not created");
        host.hud.engine = engine;
        host.hud.refresh();
        wait(1);
        return host;
    }

    function fourArmyField() {
        return [testCase.owner(1, "CPU I", 1, "roman_republic", true), testCase.owner(2, "CPU II", 2, "carthage", true), testCase.owner(3, "CPU III", 1, "roman_republic", true), testCase.owner(4, "CPU IV", 2, "carthage", true)];
    }

    function test_every_contending_army_gets_a_row() {
        var engine = engineComponent.createObject(testCase, {
                "owner_info": testCase.fourArmyField()
            });
        var host = testCase.makeHud(engine);
        compare(host.hud.board.length, 4);
        engine.destroy();
        host.destroy();
    }

    function test_an_ambient_faction_is_left_off_the_board() {
        var owners = testCase.fourArmyField();
        owners.push(testCase.owner(99, "Iron Sepulcher tomb_1", 9, "iron_sepulcher", false));
        var engine = engineComponent.createObject(testCase, {
                "owner_info": owners
            });
        var host = testCase.makeHud(engine);
        compare(host.hud.board.length, 4, "the tomb owner was listed as an army");
        engine.destroy();
        host.destroy();
    }

    function test_deaths_to_the_wild_are_told_apart_from_deaths_in_battle() {
        var engine = engineComponent.createObject(testCase, {
                "owner_info": testCase.fourArmyField()
            });
        var host = testCase.makeHud(engine);
        var wild = 0;
        for (var i = 0; i < host.hud.board.length; ++i) {
            if (host.hud.board[i].ownerId === 2)
                wild = host.hud.board[i].lossesToWild;
            else
                compare(host.hud.board[i].lossesToWild, 0);
        }
        compare(wild, 3, "an army's losses to neutral forces did not reach the board");
        engine.destroy();
        host.destroy();
    }

    function test_the_board_reports_each_armys_manpower() {
        var engine = engineComponent.createObject(testCase, {
                "owner_info": testCase.fourArmyField()
            });
        var host = testCase.makeHud(engine);
        compare(host.hud.board[0].manpower, 10);
        compare(host.hud.board[0].manpowerCap, 280);
        engine.destroy();
        host.destroy();
    }

    function test_the_board_is_grouped_by_side() {
        var engine = engineComponent.createObject(testCase, {
                "owner_info": testCase.fourArmyField()
            });
        var host = testCase.makeHud(engine);
        var teams = [];
        for (var i = 0; i < host.hud.board.length; ++i)
            teams.push(host.hud.board[i].teamId);
        compare(teams, [1, 1, 2, 2], "the armies were not grouped by side");
        engine.destroy();
        host.destroy();
    }

    function test_a_four_army_board_shows_one_side_per_row() {
        var engine = engineComponent.createObject(testCase, {
                "owner_info": testCase.fourArmyField()
            });
        var host = testCase.makeHud(engine);
        var board = findChild(host.hud, "spectatorBoard");
        verify(board !== null);
        compare(board.columns, 2, "four armies should share the board two to a row");
        engine.destroy();
        host.destroy();
    }

    function test_cycling_the_camera_walks_the_board() {
        var engine = engineComponent.createObject(testCase, {
                "owner_info": testCase.fourArmyField(),
                "selected_player_id": 1
            });
        var host = testCase.makeHud(engine);
        var asked = [];
        host.hud.follow_requested.connect(function (ownerId) {
                asked.push(ownerId);
            });
        host.hud.follow_next(1);
        compare(asked.length, 1);
        compare(asked[0], 3, "following forward should reach the next army on the board");
        host.hud.follow_next(-1);
        compare(asked[1], 4, "following back from the first army should wrap to the last");
        engine.destroy();
        host.destroy();
    }

    function test_full_commander_names_fit_on_a_wide_screen() {
        var owners = [testCase.owner(1, "Quintus Fabius Maximus", 1, "roman_republic", true), testCase.owner(2, "Hannibal Barca of Carthage", 2, "carthage", true), testCase.owner(3, "Publius Cornelius Scipio", 1, "roman_republic", true), testCase.owner(4, "Hasdrubal Barca", 2, "carthage", true)];
        var engine = engineComponent.createObject(testCase, {
                "owner_info": owners
            });
        var host = hostComponent.createObject(testCase, {
                "width": 1920,
                "height": 360
            });
        host.hud.engine = engine;
        host.hud.refresh();
        wait(50);
        var board = findChild(host.hud, "spectatorBoard");
        var seen = 0;
        for (var i = 0; i < board.contentItem.children.length; ++i) {
            var row = board.contentItem.children[i];
            if (row.objectName !== "spectatorArmyRow")
                continue;
            var label = findChild(row, "spectatorArmyName");
            verify(label !== null);
            verify(!label.truncated, "'" + label.text + "' was elided with room to spare");
            ++seen;
        }
        compare(seen, 4);
        engine.destroy();
        host.destroy();
    }

    Component {
        id: engineComponent

        QtObject {
            id: fakeEngine

            property var owner_info: []
            property int selected_player_id: 1
            property bool is_spectator_mode: true

            function get_player_stats(ownerId) {
                return {
                    "enemiesKilled": ownerId,
                    "losses": ownerId * 2,
                    "lossesToWild": ownerId === 2 ? 3 : 0,
                    "troopsRecruited": ownerId * 3,
                    "barracksOwned": ownerId,
                    "playTimeSec": 60,
                    "gameEnded": false
                };
            }
        }
    }

    Component {
        id: hostComponent

        Item {
            property alias hud: spectator

            HUDBottomSpectator {
                id: spectator

                anchors.fill: parent
            }
        }
    }
}
