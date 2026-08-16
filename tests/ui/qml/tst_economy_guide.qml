import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "EconomyGuide"
    when: windowShown
    width: 1000
    height: 700
    visible: true

    readonly property var woodEntry: ({
            "key": "wood",
            "amount": 120,
            "harvested": 80,
            "gatherable": true,
            "yield_per_trip": 40,
            "gathering_workers": 2,
            "carrying": 40,
            "display_cap": 640,
            "tradeable": true,
            "used_by": ["home", "barracks", "archer"],
            "shortfall": 30,
            "shortfall_item": "barracks",
            "relevant": true
        })

    readonly property var foodEntry: ({
            "key": "food",
            "amount": 380,
            "harvested": 0,
            "gatherable": false,
            "yield_per_trip": 0,
            "gathering_workers": 0,
            "carrying": 0,
            "display_cap": 0,
            "tradeable": true,
            "used_by": [],
            "shortfall": 0,
            "shortfall_item": "",
            "relevant": true
        })

    readonly property var mockEconomy: ({
            "resources": [testCase.foodEntry, testCase.woodEntry],
            "help": {
                "buildings": [{
                        "item_type": "home",
                        "resource_costs": {
                            "wood": 50,
                            "stone": 15
                        },
                        "build_time": 10,
                        "missing": {
                            "stone": 5
                        },
                        "affordable": false,
                        "prerequisite": "builder",
                        "prerequisite_met": true
                    }],
                "units": [{
                        "unit_type": "archer",
                        "display_name": "Archer",
                        "population_cost": 50,
                        "resource_costs": {
                            "wood": 6
                        },
                        "build_time": 5,
                        "individuals_per_unit": 20,
                        "missing": {},
                        "affordable": true,
                        "prerequisite": "barracks",
                        "prerequisite_met": false,
                        "manpower_met": true,
                        "population_met": true
                    }],
                "builder_count": 2,
                "idle_builder_count": 1,
                "barracks_count": 0,
                "population": 120,
                "population_cap": 600,
                "home_population_bonus": 50,
                "civilian_delivery_grant": 50
            },
            "coach": {
                "step": "gather",
                "step_index": 0,
                "step_count": 4,
                "steps": [{
                        "id": "gather",
                        "done": false
                    }, {
                        "id": "build",
                        "done": false
                    }, {
                        "id": "recruit",
                        "done": false
                    }, {
                        "id": "army",
                        "done": false
                    }],
                "complete": false,
                "builder_count": 2
            },
            "coach_visible": true
        })

    function test_every_resource_has_a_label_a_source_and_a_glyph() {
        var keys = EconomyGuide.resourceOrder;
        compare(keys.length, 5, "the guide must cover every resource the engine tracks");
        for (var i = 0; i < keys.length; ++i) {
            var key = keys[i];
            verify(EconomyGuide.resource_label(key) !== key, key + " has no translated label");
            verify(EconomyGuide.resource_source(key).length > 0, key + " does not say where it comes from");
            verify(Icons.resourceGlyph(key) !== "", key + " has no glyph fallback");
        }
    }

    function test_food_is_part_of_the_vocabulary() {
        verify(EconomyGuide.resourceOrder.indexOf("food") >= 0);
        verify(Icons.resource("food").toString() !== "", "food has no artwork");
    }

    function test_a_resource_tooltip_names_source_use_storage_state_and_deficit() {
        var text = EconomyGuide.resource_tooltip(testCase.woodEntry);
        verify(text.indexOf("120") >= 0, "the amount is missing: " + text);
        verify(text.indexOf("Collect") >= 0, "the source is missing: " + text);
        verify(text.indexOf("Barracks") >= 0, "what it is spent on is missing: " + text);
        verify(text.indexOf("640") >= 0, "the storage line is missing: " + text);
        verify(text.indexOf("2") >= 0, "the gather state is missing: " + text);
        verify(text.indexOf("30") >= 0, "the deficit is missing: " + text);
    }

    function test_a_resource_with_no_gathering_says_so() {
        var idle = JSON.parse(JSON.stringify(testCase.woodEntry));
        idle.gathering_workers = 0;
        idle.carrying = 0;
        verify(EconomyGuide.gather_state_line(idle).length > 0);
        compare(EconomyGuide.gather_state_line(testCase.foodEntry), "", "food is not gathered");
    }

    function test_a_missing_summary_names_the_resource_and_the_amount() {
        var text = EconomyGuide.missing_summary({
                "stone": 5,
                "wood": 12
            });
        verify(text.indexOf("5") >= 0 && text.indexOf("Stone") >= 0, text);
        verify(text.indexOf("12") >= 0 && text.indexOf("Wood") >= 0, text);
    }

    function test_a_cost_summary_covers_every_resource_including_food() {
        var text = EconomyGuide.cost_summary({
                "food": 20,
                "gold": 30
            });
        verify(text.indexOf("Food") >= 0, text);
        verify(text.indexOf("Gold") >= 0, text);
        compare(EconomyGuide.cost_summary({}), qsTr("No resource cost"));
    }

    function test_every_coach_step_has_a_title_and_a_body() {
        var steps = ["gather", "build", "recruit", "army", "done"];
        for (var i = 0; i < steps.length; ++i) {
            verify(EconomyGuide.coach_title(steps[i]).length > 0, steps[i]);
            verify(EconomyGuide.coach_body(steps[i], {
                        "builder_count": 1
                    }).length > 0, steps[i]);
        }
    }

    function test_the_coach_asks_for_a_builder_before_it_asks_for_a_gather_order() {
        var without = EconomyGuide.coach_body("gather", {
                "builder_count": 0
            });
        var with_builder = EconomyGuide.coach_body("gather", {
                "builder_count": 3
            });
        verify(without !== with_builder);
        verify(without.indexOf("builder") >= 0);
    }

    function test_the_coach_panel_shows_the_current_step() {
        var coach = coachComponent.createObject(testCase, {
                "economy": testCase.mockEconomy
            });
        wait(50);
        verify(coach !== null);
        verify(coach.height > 0);
        compare(coach.step, "gather");
        compare(coach.stepCount, 4);
        coach.destroy();
    }

    function test_the_help_panel_lists_costs_and_the_reason_an_item_is_blocked() {
        var panel = helpComponent.createObject(testCase, {
                "economy": testCase.mockEconomy
            });
        wait(50);
        verify(panel !== null);
        compare(panel.buildings.length, 1);
        compare(panel.units.length, 1);
        var blockedBuilding = panel.blocked_reason(panel.buildings[0], "building");
        verify(blockedBuilding.indexOf("Stone") >= 0, blockedBuilding);
        var blockedUnit = panel.blocked_reason(panel.units[0], "unit");
        verify(blockedUnit.indexOf("barracks") >= 0, blockedUnit);
        verify(panel.population_line().indexOf("600") >= 0, panel.population_line());
        verify(panel.builder_line().indexOf("2") >= 0, panel.builder_line());
        panel.destroy();
    }

    Component {
        id: coachComponent

        EconomyCoach {
            width: 320
        }
    }

    Component {
        id: helpComponent

        EconomyHelpPanel {
            width: 900
            height: 600
        }
    }
}
