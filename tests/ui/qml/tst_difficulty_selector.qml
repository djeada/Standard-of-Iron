import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import StandardOfIron.Core 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "DifficultySelector"
    when: windowShown
    width: 900
    height: 400
    visible: true

    readonly property var presets: [{
            "id": "easy",
            "resource_multiplier": 0.8,
            "unit_multiplier": 0.8,
            "wave_multiplier": 0.75,
            "is_baseline": false
        }, {
            "id": "normal",
            "resource_multiplier": 1,
            "unit_multiplier": 1,
            "wave_multiplier": 1,
            "is_baseline": true
        }, {
            "id": "hard",
            "resource_multiplier": 1.5,
            "unit_multiplier": 1.5,
            "wave_multiplier": 1.5,
            "is_baseline": false
        }, {
            "id": "very_hard",
            "resource_multiplier": 2,
            "unit_multiplier": 2,
            "wave_multiplier": 2,
            "is_baseline": false
        }]

    readonly property var campaign: ({
            "id": "second_punic_war",
            "title": "The Barcid Road",
            "description": "From the Rhone to Zama.",
            "missions": [{
                    "mission_id": "crossing_the_rhone",
                    "title": "Crossing the Rhone",
                    "description": "Force the river.",
                    "unlocked": true,
                    "completed": false,
                    "difficulty_modifier": 1,
                    "region_id": "rhone"
                }]
        })

    Component {
        id: campaignComponent

        CampaignScreen {
            width: 1280
            height: 800
        }
    }

    Component {
        id: missionsComponent

        MissionsScreen {
            width: 1280
            height: 800
        }
    }

    Component {
        id: selectorComponent

        DifficultySelector {
            width: 880
            presets: testCase.presets
        }
    }

    function make_selector(selected) {
        var selector = selectorComponent.createObject(testCase, {
                "selected_id": selected || "normal"
            });
        verify(selector !== null, "DifficultySelector failed to instantiate");
        wait(30);
        return selector;
    }

    function test_the_catalog_lists_the_four_presets_in_rising_order() {
        var ids = DifficultyCatalog.entries.map(function (entry) {
                return entry.id;
            });
        compare(ids, ["easy", "normal", "hard", "very_hard"]);
        compare(DifficultyCatalog.defaultId, "normal");
    }

    function test_every_preset_names_itself_and_carries_its_own_drawing() {
        var names = [];
        var icons = [];
        for (var i = 0; i < DifficultyCatalog.entries.length; i++) {
            var entry = DifficultyCatalog.entries[i];
            verify(entry.name.length > 0, entry.id + " has no display name");
            verify(entry.quip.length > 0, entry.id + " has no tooltip line");
            verify(IconArt.has(entry.icon), entry.id + " has no icon art: " + entry.icon);
            verify(names.indexOf(entry.name) === -1, "two presets share a name");
            verify(icons.indexOf(entry.icon) === -1, "two presets share an icon");
            names.push(entry.name);
            icons.push(entry.icon);
        }
    }

    function test_brutal_is_the_face_of_very_hard() {
        compare(DifficultyCatalog.name_for("very_hard"), qsTr("Brutal"));
        compare(DifficultyCatalog.entry("nonsense").id, "normal");
    }

    function test_the_bonus_summary_states_the_exact_numbers() {
        compare(DifficultyCatalog.signed_percent(1), "±0%");
        compare(DifficultyCatalog.signed_percent(1.5), "+50%");
        compare(DifficultyCatalog.signed_percent(2), "+100%");
        compare(DifficultyCatalog.signed_percent(0.8), "−20%");
        var hard = DifficultyCatalog.summary_for(testCase.presets, "hard");
        verify(hard.indexOf("+50%") !== -1, "hard summary hides its numbers: " + hard);
        var easy = DifficultyCatalog.summary_for(testCase.presets, "easy");
        verify(easy.indexOf("−20%") !== -1, "easy summary hides its numbers: " + easy);
        verify(easy.indexOf("−25%") !== -1, "easy summary hides its wave number: " + easy);
    }

    function test_arrow_keys_walk_the_presets_and_report_the_choice() {
        var selector = make_selector("normal");
        var chosen = [];
        selector.chosen.connect(function (id) {
                chosen.push(id);
            });
        selector.forceActiveFocus();
        keyClick(Qt.Key_Right);
        compare(selector.selected_id, "hard");
        keyClick(Qt.Key_Right);
        compare(selector.selected_id, "very_hard");
        keyClick(Qt.Key_Right);
        compare(selector.selected_id, "very_hard", "the last preset is the end of the row");
        keyClick(Qt.Key_Left);
        keyClick(Qt.Key_Left);
        keyClick(Qt.Key_Left);
        compare(selector.selected_id, "easy");
        keyClick(Qt.Key_Left);
        compare(selector.selected_id, "easy", "the first preset is the start of the row");
        compare(chosen, ["hard", "very_hard", "hard", "normal", "easy"]);
        selector.destroy();
    }

    function test_the_campaign_screen_hands_the_chosen_preset_to_the_launch() {
        var screen = campaignComponent.createObject(testCase, {
                "campaigns": [testCase.campaign],
                "current_campaign": testCase.campaign,
                "selected_mission_index": 0
            });
        verify(screen !== null, "CampaignScreen failed to instantiate");
        wait(60);
        var launches = [];
        screen.mission_selected.connect(function (campaign_id, mission_id, difficulty) {
                launches.push([campaign_id, mission_id, difficulty]);
            });
        screen.selected_difficulty = "very_hard";
        var panel = findChild(screen, "missionDetailPanel");
        verify(panel !== null, "the mission detail panel must be on the war table");
        compare(panel.difficulty_id, "very_hard", "the panel reports the chosen preset");
        panel.start_mission_clicked();
        compare(launches.length, 1);
        compare(launches[0][1], "crossing_the_rhone");
        compare(launches[0][2], "very_hard", "the launch carries the chosen preset");
        screen.destroy();
    }

    function test_the_missions_screen_hands_the_chosen_preset_to_the_launch() {
        var screen = missionsComponent.createObject(testCase, {
                "missions": [{
                        "title": "Hold the Sallow Ford",
                        "file_path": "missions/sallow.json",
                        "summary": "Hold the crossing.",
                        "objectives": [],
                        "optional_objectives": [],
                        "defeat_conditions": [],
                        "starting_force": [],
                        "starting_resources": []
                    }],
                "selected_index": 0
            });
        verify(screen !== null, "MissionsScreen failed to instantiate");
        wait(60);
        var launches = [];
        screen.mission_chosen.connect(function (file_path, difficulty) {
                launches.push([file_path, difficulty]);
            });
        var selector = findChild(screen, "missionDifficultySelector");
        verify(selector !== null, "the missions screen must offer a difficulty selector");
        selector.forceActiveFocus();
        keyClick(Qt.Key_Right);
        compare(screen.selected_difficulty, "hard");
        var badge = findChild(screen, "missionDifficultyBadge");
        verify(badge !== null, "the deploy row must show the chosen preset");
        compare(badge.difficulty_id, "hard");
        screen.start_selected();
        compare(launches.length, 1);
        compare(launches[0][1], "hard", "the launch carries the chosen preset");
        screen.destroy();
    }

    function test_choosing_the_preset_already_chosen_reports_nothing() {
        var selector = make_selector("easy");
        var chosen = 0;
        selector.chosen.connect(function () {
                chosen++;
            });
        selector.forceActiveFocus();
        keyClick(Qt.Key_Left);
        compare(chosen, 0);
        selector.destroy();
    }
}
