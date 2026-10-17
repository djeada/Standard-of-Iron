import QtQuick 2.15
import QtQuick.Window 2.15
import QtTest 1.15
import StandardOfIron 1.0
import StandardOfIron.Core 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "FrontEndScreens"
    when: windowShown
    width: 1280
    height: 720
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
            "description": "Command Hannibal's Carthaginian army from the Alpine invasion of 218 BC to Zama in 202 BC. Break Roman camps, take fortified towns, preserve allied economies, and survive the Republic's answering legions while the Iron Sepulcher watches from roads older than either empire.",
            "missions": testCase.campaign_missions()
        })

    function campaign_missions() {
        var titles = ["Crossing the Rhone", "Crossing the Alps", "Battle of Ticino", "Battle of Trebia", "Battle of Lake Trasimene", "Battle of Cannae", "The Campanian Vigil", "Battle of Zama"];
        var missions = [];
        for (var i = 0; i < titles.length; ++i)
            missions.push({
                    "mission_id": "mission_" + i,
                    "title": titles[i],
                    "order_index": i,
                    "intro_text": "218 BC: near the Ticinus, Scipio rides out with his javelin-men and Gallic horse in front to test Hannibal's cavalry.",
                    "difficulty_modifier": 1.1,
                    "world_region_id": "region_" + i,
                    "unlocked": i < 3,
                    "completed": i < 2
                });
        return missions;
    }

    readonly property var missions: [{
            "title": "Hold the Sallow Ford",
            "file_path": "missions/sallow.json",
            "summary": "Hold the crossing.",
            "objectives": [{
                    "description": "Hold the ford."
                }],
            "completed": true
        }, {
            "title": "The Siege of Aurelia Magna",
            "file_path": "missions/aurelia.json",
            "summary": "Break all six assaults.",
            "objectives": [{
                    "description": "Break all six assaults."
                }]
        }, {
            "title": "The Storming of Victumulae",
            "file_path": "missions/victumulae.json",
            "summary": "Take the town.",
            "objectives": [{
                    "description": "Take the town."
                }]
        }]

    function luminance(c) {
        function channel(v) {
            return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
        }
        return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
    }

    function contrast(a, b) {
        var la = luminance(a);
        var lb = luminance(b);
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }

    function over(top, bottom) {
        return Qt.rgba(top.r * top.a + bottom.r * (1 - top.a), top.g * top.a + bottom.g * (1 - top.a), top.b * top.a + bottom.b * (1 - top.a), 1);
    }

    function visible_texts(item, out) {
        if (!item || !item.visible)
            return out;
        if (typeof item.text === "string" && item.truncated !== undefined)
            out.push(item);
        var kids = item.children;
        if (!kids)
            return out;
        for (var i = 0; i < kids.length; ++i)
            visible_texts(kids[i], out);
        return out;
    }

    function make(component, props) {
        var host = hostComponent.createObject(testCase, {
                "width": 1280,
                "height": 720
            });
        var screen = component.createObject(host, props || {});
        verify(screen !== null);
        wait(120);
        return host;
    }

    function test_settings_sliders_show_arabic_percentages() {
        var host = make(settingsComponent);
        var screen = host.children[0];
        compare(screen.percent_text(70), "70%");
        compare(screen.percent_text(45.2), "45%");
        var texts = visible_texts(screen, []);
        var seen_percent = false;
        for (var i = 0; i < texts.length; ++i) {
            verify(!/^[IVXLCDMN]+%$/.test(texts[i].text), "a slider still reads in Roman numerals: " + texts[i].text);
            if (/^\d+%$/.test(texts[i].text))
                seen_percent = true;
        }
        verify(seen_percent, "no slider shows an Arabic percentage");
        host.destroy();
    }

    function test_window_mode_reflects_the_real_window() {
        var host = make(settingsComponent);
        var combo = findChild(host, "windowModeComboBox");
        verify(combo !== null);
        var expected = Window.window.visibility === Window.FullScreen ? "fullscreen" : "windowed";
        compare(combo.currentText, expected, "the combo must show what the window is, not the saved preference");
        host.destroy();
    }

    function test_camera_speed_description_introduces_the_group() {
        var host = make(settingsComponent);
        var description = findChild(host, "cameraSpeedGroupDescription");
        verify(description !== null);
        verify(description.text.indexOf("three") !== -1, description.text);
        var texts = visible_texts(host, []);
        var pan = null;
        for (var i = 0; i < texts.length; ++i) {
            if (texts[i].text === qsTr("Camera pan speed:"))
                pan = texts[i];
        }
        verify(pan !== null);
        verify(description.mapToItem(null, 0, 0).y < pan.mapToItem(null, 0, 0).y, "the group description must sit above the first camera slider");
        host.destroy();
    }

    function test_settings_scrollbar_is_visible_when_the_panel_scrolls() {
        var host = make(settingsComponent);
        var bar = findChild(host, "settingsScrollBar");
        verify(bar !== null);
        verify(bar.visible && bar.width > 0, "the settings panel scrolls without a visible scrollbar");
        host.destroy();
    }

    function test_missions_counts_and_titles_read_in_full() {
        var host = make(missionsComponent, {
                "missions": testCase.missions,
                "selected_index": 1
            });
        findChild(host, "missionDifficultySelector").presets = testCase.presets;
        wait(120);
        compare(findChild(host, "missionsCompletedCount").text, "1 / 3");
        var texts = visible_texts(host, []);
        var titles = 0;
        for (var i = 0; i < texts.length; ++i) {
            if (texts[i].objectName === "missionRowTitle") {
                titles++;
                verify(!texts[i].truncated, "mission title clipped: " + texts[i].text);
            }
            if (texts[i].objectName === "difficultyCompactSummary")
                verify(!texts[i].truncated && texts[i].text.length > 0, "difficulty summary clipped: " + texts[i].text);
        }
        compare(titles, 3);
        var hotkey = findChild(host, "missionDeployHotkey");
        var badge = findChild(host, "missionDifficultyBadge");
        verify(badge.x >= hotkey.x + hotkey.width, "the difficulty badge is drawn over the Enter hint");
        host.destroy();
    }

    function test_campaign_war_table_fits_720p() {
        var host = make(campaignComponent, {
                "campaigns": [testCase.campaign],
                "current_campaign": testCase.campaign,
                "selected_mission_index": 2
            });
        findChild(host, "campaignDifficultySelector").presets = testCase.presets;
        wait(200);
        compare(findChild(host, "campaignProgressCount").text, "2 / 8");
        var description = findChild(host, "campaignDescription");
        verify(!description.truncated, "the campaign description is cut off");
        var map = findChild(host, "campaignMapPanel");
        verify(map.height >= 240, "the war table map is a strip: " + map.height);
        var legend = findChild(host, "campaignMapLegend");
        verify(legend.mapToItem(map, 0, 0).y >= 0, "the legend is clipped at the top of the map");
        var hint = findChild(host, "campaignMapControlsHint");
        verify(!hint.visible, "the controls hint covers the map until asked for");
        var list = findChild(host, "campaignMissionList");
        verify(list.height > 300, "the chronicle shows a single mission card: " + list.height);
        verify(findChild(host, "campaignMissionListScrollBar").visible, "the chronicle scrolls without a scrollbar");
        var detail = findChild(host, "missionDetailPanel");
        verify(detail.mapToItem(host, 0, detail.height).y <= host.height, "the mission detail spills off the screen");
        var texts = visible_texts(host, []);
        for (var i = 0; i < texts.length; ++i) {
            if (texts[i].objectName === "difficultyCompactSummary")
                verify(!texts[i].truncated, "difficulty summary clipped: " + texts[i].text);
        }
        host.destroy();
    }

    function test_save_rows_show_a_readable_title_and_date() {
        var row = slotRowComponent.createObject(testCase, {
                "width": 820,
                "slot_name": "autosave_3",
                "title": "autosave_3",
                "kind": "autosave",
                "timestamp": "2026-10-16T22:20:00",
                "play_time_seconds": 1080
            });
        wait(30);
        compare(findChild(row, "saveSlotTitle").text, qsTr("Autosave 3"));
        compare(row.slot_name, "autosave_3", "operations keep the raw slot id");
        compare(row.human_title("quicksave", "quicksave"), qsTr("Quicksave"));
        compare(row.human_title("Before the ford", "Before the ford"), "Before the ford");
        var card = Theme.cardBase;
        var states = [false, true];
        for (var i = 0; i < states.length; ++i) {
            row.selected = states[i];
            var background = over(row.color, card);
            var saved = findChild(row, "saveSlotSavedAt");
            verify(contrast(saved.color, background) >= 4.5, "the saved-at line is unreadable (selected=" + states[i] + "): " + contrast(saved.color, background).toFixed(2));
        }
        row.destroy();
    }

    function test_field_manual_tabs_fit_720p() {
        var tabs = [4, 5];
        for (var t = 0; t < tabs.length; ++t) {
            var host = make(helpComponent, {
                    "current_tab": tabs[t]
                });
            wait(120);
            for (var i = 0; i < 9; ++i) {
                var tab = findChild(host, "fieldManualTab" + i);
                verify(tab !== null, "tab " + i + " missing");
                verify(!tab.contentItem.truncated, "tab label clipped: " + tab.text);
            }
            host.destroy();
        }
    }

    function test_in_match_menu_leads_with_resume_and_hides_start_here() {
        var host = make(menuComponent, {
                "game_started": true,
                "can_restart": true
            });
        var menu = host.children[0];
        verify(!menu.tutorial_pending, "START HERE must not show during a battle");
        compare(menu.index_of_entry("resume"), 0, "Resume battle comes first in the battle menu");
        verify(menu.index_of_entry("restart") > 0);
        verify(menu.index_of_entry("abandon") > 0);
        compare(menu.index_of_entry("continue"), -1, "the save-loading Continue is not offered mid-battle");
        var restart = restartSpy.createObject(testCase, {
                "target": menu,
                "signalName": "restart_requested"
            });
        var abandon = restartSpy.createObject(testCase, {
                "target": menu,
                "signalName": "abandon_requested"
            });
        menu.trigger_selection(menu.index_of_entry("restart"));
        menu.trigger_selection(menu.index_of_entry("abandon"));
        compare(restart.count, 1);
        compare(abandon.count, 1);
        menu.game_started = false;
        wait(30);
        compare(menu.index_of_entry("resume"), -1, "the front-end menu has no battle to resume");
        compare(menu.index_of_entry("abandon"), -1);
        restart.destroy();
        abandon.destroy();
        host.destroy();
    }

    function test_start_here_hides_once_the_tutorial_was_started() {
        var host = make(menuComponent, {
                "game_started": false
            });
        var menu = host.children[0];
        var pending = !UiPreferences.tutorialCompleted;
        compare(menu.tutorial_pending, pending);
        menu.tutorial_started = true;
        verify(!menu.tutorial_pending);
        host.destroy();
    }

    function test_menu_tagline_sits_on_a_scrim() {
        var host = make(menuComponent, {
                "game_started": false
            });
        var tagline = findChild(host, "mainMenuTagline");
        var scrim = findChild(host, "mainMenuTaglineScrim");
        verify(tagline !== null && scrim !== null);
        verify(scrim.width >= tagline.contentWidth, "the scrim must cover the tagline");
        verify(contrast(tagline.color, over(scrim.color, Qt.rgba(0.35, 0.3, 0.25, 1))) >= 4.5, "the tagline is unreadable even over bright art");
        host.destroy();
    }

    Component {
        id: hostComponent

        Item {
        }
    }

    Component {
        id: settingsComponent

        SettingsPanel {
        }
    }

    Component {
        id: missionsComponent

        MissionsScreen {
        }
    }

    Component {
        id: campaignComponent

        CampaignScreen {
        }
    }

    Component {
        id: helpComponent

        HelpPanel {
        }
    }

    Component {
        id: menuComponent

        MainMenu {
        }
    }

    Component {
        id: slotRowComponent

        SaveSlotRow {
        }
    }

    Component {
        id: restartSpy

        SignalSpy {
        }
    }
}
