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

    readonly property var four_bases: [{
            "key": "p1_barracks",
            "name": "South-West",
            "defaultPlayerId": 1,
            "previewX": 0.15,
            "previewY": 0.85
        }, {
            "key": "p2_barracks",
            "name": "North-East",
            "defaultPlayerId": 2,
            "previewX": 0.85,
            "previewY": 0.15
        }, {
            "key": "north_toll_barracks",
            "name": "North Toll",
            "defaultPlayerId": 0,
            "previewX": 0.5,
            "previewY": 0.2
        }, {
            "key": "south_toll_barracks",
            "name": "South Toll",
            "defaultPlayerId": 0,
            "previewX": 0.5,
            "previewY": 0.8
        }]

    function seated_screen() {
        var screen = make_screen(two_slot_map);
        screen.map_bases = four_bases;
        screen.reseat_bases();
        screen.update_validation_error();
        return screen;
    }

    function test_each_seat_opens_on_the_base_the_map_authored_for_it() {
        var screen = seated_screen();
        compare(screen.roster.get(0).baseKey, "p1_barracks", "slot one should open on its own camp");
        compare(screen.roster.get(1).baseKey, "p2_barracks", "slot two should open on its own camp");
        compare(screen.roster.get(0).baseName, "South-West", "the chip has to name the base");
        compare(screen.validation_error, "", "the authored seating is startable");
        screen.destroy();
    }

    function test_cycling_a_base_only_ever_lands_on_a_free_one() {
        var screen = seated_screen();
        var taken = screen.roster.get(1).baseKey;
        for (var step = 0; step < 6; step++) {
            screen.cycle_player_base(0);
            var mine = screen.roster.get(0).baseKey;
            verify(mine !== "", "a seat must never be left without a base");
            verify(mine !== taken, "cycling handed a seat a base another player holds");
            compare(screen.roster.get(1).baseKey, taken, "cycling one seat moved another");
        }
        screen.destroy();
    }

    function test_claiming_a_held_base_swaps_the_two_seats() {
        var screen = seated_screen();
        screen.set_player_base(0, "p2_barracks");
        compare(screen.roster.get(0).baseKey, "p2_barracks", "the claim did not go through");
        compare(screen.roster.get(1).baseKey, "p1_barracks", "the displaced seat was left without a base");
        compare(screen.validation_error, "", "a swap must leave the match startable");
        screen.destroy();
    }

    function test_a_map_marker_seats_the_highlighted_player() {
        var screen = seated_screen();
        screen.focus_player(1);
        screen.claim_base("south_toll_barracks");
        compare(screen.roster.get(1).baseKey, "south_toll_barracks", "the marker did not reseat the highlighted seat");
        compare(screen.roster.get(0).baseKey, "p1_barracks", "the marker reseated the wrong player");
        screen.destroy();
    }

    function test_a_dropped_seat_cannot_be_the_one_a_marker_reseats() {
        var screen = seated_screen();
        screen.focus_player(1);
        screen.toggle_player_enabled(1);
        screen.claim_base("north_toll_barracks");
        compare(screen.roster.get(1).baseKey, "p2_barracks", "a seat out of the battle was reseated");
        compare(screen.roster.get(0).baseKey, "north_toll_barracks", "the marker did not fall through to a seated player");
        screen.destroy();
    }

    function test_the_chosen_base_travels_with_the_match_configuration() {
        var screen = seated_screen();
        screen.set_player_base(0, "north_toll_barracks");
        var configs = screen.get_player_configs();
        compare(configs.length, 2);
        compare(configs[0].baseKey, "north_toll_barracks", "the match was started without the chosen base");
        compare(configs[1].baseKey, "p2_barracks");
        screen.destroy();
    }

    function test_a_seat_without_a_base_cannot_start_the_match() {
        var screen = seated_screen();
        screen.apply_base(0, "");
        screen.update_validation_error();
        verify(!screen.can_start(), "a match started with a player who has no camp");
        verify(screen.validation_error !== "", "the reason must be shown to the player");
        screen.destroy();
    }

    function test_moving_the_human_seat_takes_its_camp_along() {
        var screen = seated_screen();
        screen.cycle_human_slot();
        compare(screen.roster.get(0).player_id, 2, "the human did not move slot");
        compare(screen.roster.get(0).baseKey, "p2_barracks", "moving seat left the human at the old camp");
        compare(screen.roster.get(1).baseKey, "p1_barracks", "the displaced CPU did not take the vacated camp");
        screen.destroy();
    }

    function findCards(item, found) {
        var out = found !== undefined ? found : [];
        if (item === null || item === undefined)
            return out;
        var kids = item.children;
        for (var i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === "rosterSeatCard")
                out.push(kids[i]);
            testCase.findCards(kids[i], out);
        }
        return out;
    }

    function test_a_dropped_seat_looks_dropped_whoever_owns_it() {
        var screen = make_screen(two_slot_map);
        wait(50);
        var cards = testCase.findCards(screen);
        compare(cards.length, 2, "every seat needs a card to read");
        var humanCard = cards[0];
        var cpuCard = cards[1];
        var humanBorder = humanCard.border.width;
        verify(humanBorder > cpuCard.border.width, "the human seat should stand out while it is in the battle");
        screen.toggle_player_enabled(0);
        screen.toggle_player_enabled(1);
        wait(50);
        cards = testCase.findCards(screen);
        compare(cards[0].opacity, cards[1].opacity, "a dropped human seat stayed brighter than a dropped CPU seat");
        compare(cards[0].border.width, cards[1].border.width, "a dropped human seat kept its accent border");
        screen.destroy();
    }

    function test_a_two_camp_map_can_be_observed_without_touching_the_roster() {
        var screen = make_screen(two_slot_map);
        verify(screen.can_observe(), "a two camp battlefield should be watchable");
        var watched = [];
        screen.observe_requested.connect(function (path) {
                watched.push(path);
            });
        screen.observe_selection();
        compare(watched.length, 1, "the observe button did not ask for a match");
        compare(watched[0], two_slot_map[0].path);
        compare(screen.roster.count, 2, "observing must not disturb the roster");
        verify(screen.roster.get(0).isHuman, "observing must not unseat the player");
        screen.destroy();
    }

    function test_a_solo_battlefield_cannot_be_observed() {
        var screen = make_screen(solo_map);
        verify(!screen.can_observe(), "a one camp battlefield has nothing to watch");
        var watched = [];
        screen.observe_requested.connect(function (path) {
                watched.push(path);
            });
        screen.observe_selection();
        compare(watched.length, 0, "a solo map started an observed match anyway");
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
