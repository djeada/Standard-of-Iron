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
                "population": id * 10,
                "population_cap": 280
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
