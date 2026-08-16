import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "MapSelect"
    when: windowShown
    width: 1280
    height: 800
    visible: true

    readonly property var two_slot_map: [{
            "name": "Forest Battlefield",
            "description": "Two active players.",
            "path": "assets/maps/forest.json",
            "playerCount": 2,
            "soloPlayable": false,
            "player_ids": [1, 2]
        }]

    readonly property var solo_map: [{
            "name": "Iron Sepulcher Watch",
            "description": "Scripted opposition.",
            "path": "assets/maps/sepulcher.json",
            "playerCount": 1,
            "soloPlayable": true,
            "player_ids": [1]
        }]

    function make_screen(maps) {
        var screen = screenComponent.createObject(testCase, {
                "maps_model": maps
            });
        verify(screen !== null, "MapSelect failed to instantiate");
        wait(50);
        screen.select_map(0);
        return screen;
    }

    function test_selecting_a_two_slot_map_seats_a_human_and_an_opponent() {
        var screen = make_screen(two_slot_map);
        compare(screen.roster.count, 2, "one human and one CPU should be seated");
        var human = screen.roster.get(0);
        var cpu = screen.roster.get(1);
        verify(human.isHuman, "the first seat belongs to the player");
        verify(!cpu.isHuman, "the second seat belongs to the AI");
        compare(human.player_id, 1);
        compare(cpu.player_id, 2);
        verify(human.team_id !== cpu.team_id, "the default seats must oppose each other");
        compare(screen.validation_error, "", "a default two-player setup is startable");
        verify(screen.can_start());
        screen.destroy();
    }

    function test_team_numbers_start_at_one_and_stay_within_the_roster() {
        var screen = make_screen(two_slot_map);
        var seen = [];
        for (var step = 0; step < 4; step++) {
            var team = screen.roster.get(0).team_id;
            verify(team >= 0 && team < 2, "a two-player match only offers two teams, saw " + team);
            if (seen.indexOf(team) === -1)
                seen.push(team);
            compare(screen.roster.get(0).teamIcon, Theme.teamIcons[team + 1], "the team mark must match the team number");
            screen.cycle_player_team(0);
        }
        compare(seen.length, 2, "cycling must reach every team");
        screen.destroy();
    }

    function test_dropping_the_opponent_blocks_the_start_on_a_versus_map() {
        var screen = make_screen(two_slot_map);
        screen.toggle_player_enabled(1);
        compare(screen.enabled_player_count(), 1);
        verify(!screen.can_start(), "a versus map cannot start with a single player");
        verify(screen.validation_error !== "", "the reason must be shown to the player");
        screen.toggle_player_enabled(1);
        verify(screen.can_start(), "re-enabling the opponent restores a startable match");
        compare(screen.validation_error, "");
        screen.destroy();
    }

    function test_putting_both_players_on_one_team_blocks_the_start() {
        var screen = make_screen(two_slot_map);
        screen.cycle_player_team(1);
        compare(screen.roster.get(0).team_id, screen.roster.get(1).team_id, "both seats now share a team");
        verify(!screen.can_start(), "a match needs two sides");
        verify(screen.validation_error !== "");
        screen.destroy();
    }

    function test_a_scripted_map_starts_with_a_single_player() {
        var screen = make_screen(solo_map);
        compare(screen.roster.count, 1, "a one-slot map seats only the player");
        verify(screen.can_start(), "scripted opposition allows a solo start");
        compare(screen.validation_error, "");
        screen.destroy();
    }

    function test_adding_a_cpu_stops_at_the_last_authored_slot() {
        var screen = make_screen(two_slot_map);
        screen.add_cpu();
        compare(screen.roster.count, 2, "the map only authors two slots");
        screen.destroy();
    }

    function test_moving_seats_swaps_the_player_with_the_slot_holder() {
        var screen = make_screen(two_slot_map);
        screen.cycle_human_slot();
        var human = screen.roster.get(0);
        var cpu = screen.roster.get(1);
        compare(human.player_id, 2, "the player took the second slot");
        compare(cpu.player_id, 1, "the AI inherited the vacated slot");
        verify(human.isHuman);
        verify(!cpu.isHuman);
        verify(screen.can_start(), "swapping seats keeps the match startable");
        screen.destroy();
    }

    function test_clearing_the_selection_empties_the_roster() {
        var screen = make_screen(two_slot_map);
        screen.select_map(-1);
        compare(screen.roster.count, 0);
        verify(!screen.can_start());
        screen.destroy();
    }

    Component {
        id: screenComponent

        MapSelect {
        }
    }
}
