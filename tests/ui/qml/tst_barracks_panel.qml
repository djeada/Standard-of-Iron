import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import StandardOfIron.Core 1.0 as Core
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "BarracksPanel"
    when: windowShown
    width: 700
    height: 400
    visible: true

    function init() {
        Core.UiPreferences.reset_to_defaults();
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    QtObject {
        id: longNamedProduction

        function has_selected_type(type) {
            return type === "barracks";
        }

        function selected_state() {
            return {
                "has_barracks": true,
                "max_units": 500,
                "queue_size": 2,
                "in_progress": true,
                "production_queue": ["swordsman", "spearman"],
                "product_type": "archer",
                "manpower_available": 120,
                "build_time": 10,
                "time_remaining": 4.5,
                "nation_id": "carthage"
            };
        }

        function unit_info(unitType, nationId) {
            var names = {
                "archer": "Libyan Archer",
                "swordsman": "Citizen Infantry",
                "spearman": "Liby-Phoenician Spear",
                "horse_swordsman": "Numidian Cavalry",
                "builder": "Phoenician Craftsman",
                "elephant": "African War Elephant"
            };
            var reserve = {
                "archer": 25,
                "swordsman": 48,
                "spearman": 41,
                "horse_swordsman": 75,
                "horse_archer": 60,
                "horse_spearman": 70,
                "builder": 28,
                "elephant": 175
            };
            var resources = {
                "archer": {
                    "wood": 30,
                    "iron": 10
                },
                "swordsman": {
                    "wood": 10,
                    "iron": 30
                },
                "spearman": {
                    "wood": 30,
                    "iron": 15
                },
                "horse_swordsman": {
                    "wood": 15,
                    "iron": 30,
                    "food": 30
                },
                "horse_archer": {
                    "wood": 25,
                    "iron": 15,
                    "food": 30
                },
                "horse_spearman": {
                    "wood": 25,
                    "iron": 22,
                    "food": 30
                },
                "builder": {
                    "wood": 20
                },
                "elephant": {
                    "wood": 40,
                    "iron": 25,
                    "food": 90
                }
            };
            return {
                "cost": reserve[unitType] || 50,
                "resource_costs": resources[unitType] || ({}),
                "build_time": 8,
                "display_name": names[unitType] || unitType
            };
        }
    }

    QtObject {
        id: fundedPlayer

        property var resources: ({
                "food": 500,
                "wood": 500,
                "stone": 500,
                "iron": 500,
                "gold": 500
            })
    }

    Component {
        id: panelComponent

        ProductionPanel {
        }
    }

    function collect(item, objectName, out) {
        if (out === undefined)
            out = [];
        if (item === null || item === undefined)
            return out;
        for (var i = 0; i < item.children.length; ++i) {
            if (item.children[i].objectName === objectName)
                out.push(item.children[i]);
            testCase.collect(item.children[i], objectName, out);
        }
        return out;
    }

    function collect_text(item, out) {
        if (out === undefined)
            out = [];
        if (item === null || item === undefined)
            return out;
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];
            if (child.visible !== false && child.contentHeight !== undefined && child.text !== undefined && String(child.text).length > 0)
                out.push(child);
            testCase.collect_text(child, out);
        }
        return out;
    }

    function make_barracks(panelWidth, panelHeight) {
        var panel = panelComponent.createObject(testCase, {
                "width": panelWidth,
                "height": panelHeight,
                "production": longNamedProduction,
                "player_state": fundedPlayer
            });
        verify(panel !== null, "the production panel was not created");
        panel.selection_tick = 1;
        tryCompare(panel, "has_barracks_selection", true);
        wait(20);
        return panel;
    }

    function in_view(item, grid) {
        var pos = item.mapToItem(grid, 0, 0);
        return pos.y >= -0.5 && pos.y + item.height <= grid.height + 0.5;
    }

    function layout_data() {
        return [{
                "tag": "1920x1080",
                "w": 634,
                "h": 224,
                "scale": 1.0
            }, {
                "tag": "1600x900",
                "w": 528,
                "h": 189,
                "scale": 1.0
            }, {
                "tag": "1280x720",
                "w": 421,
                "h": 184,
                "scale": 1.0
            }, {
                "tag": "1024x768",
                "w": 336,
                "h": 184,
                "scale": 1.0
            }, {
                "tag": "1920x1080@0.75",
                "w": 634,
                "h": 190,
                "scale": Core.UiPreferences.minUiScale
            }, {
                "tag": "1920x1080@1.5",
                "w": 634,
                "h": 302,
                "scale": 1.5
            }, {
                "tag": "1920x1080@2",
                "w": 634,
                "h": 302,
                "scale": Core.UiPreferences.maxUiScale
            }];
    }

    function test_the_recruit_grid_puts_every_card_on_one_column_grid_data() {
        return layout_data();
    }

    function test_the_recruit_grid_puts_every_card_on_one_column_grid(data) {
        Core.UiPreferences.uiScale = data.scale;
        var panel = make_barracks(data.w, data.h);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        verify(view !== undefined, "the barracks view is missing");
        var grid = testCase.collect(view, "barracksRecruitGrid")[0];
        verify(grid !== undefined, "the recruit grid is missing");
        var nameOffsets = ({});
        var badgeOffsets = ({});
        var names = testCase.collect(view, "recruitCardName");
        var shown = 0;
        for (var i = 0; i < names.length; ++i) {
            if (!testCase.in_view(names[i], grid))
                continue;
            shown++;
            var card = names[i].parent.parent;
            nameOffsets[Math.round(names[i].mapToItem(card, 0, 0).x)] = true;
            badgeOffsets[Math.round(card.width - names[i].mapToItem(card, names[i].width, 0).x)] = true;
        }
        verify(shown > 0, "no recruit card was laid out at " + data.tag);
        compare(Object.keys(nameOffsets).length, 1, "unit names start at " + Object.keys(nameOffsets).length + " different offsets at " + data.tag);
        compare(Object.keys(badgeOffsets).length, 1, "the info button gutter is not one width at " + data.tag);
        panel.destroy();
    }

    function test_cost_pairs_share_one_pitch_and_one_column_per_slot_data() {
        return layout_data();
    }

    function test_cost_pairs_share_one_pitch_and_one_column_per_slot(data) {
        Core.UiPreferences.uiScale = data.scale;
        var panel = make_barracks(data.w, data.h);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var grid = testCase.collect(view, "barracksRecruitGrid")[0];
        var rows = testCase.collect(view, "recruitCostRow");
        var pitches = ({});
        var offsets = ({});
        var widest = 0;
        for (var r = 0; r < rows.length; ++r) {
            if (!testCase.in_view(rows[r], grid))
                continue;
            var card = rows[r].parent.parent;
            var cells = 0;
            for (var c = 0; c < rows[r].children.length; ++c) {
                var cell = rows[r].children[c];
                if (cell.width === undefined || cell.width <= 0)
                    continue;
                cells++;
                pitches[Math.round(cell.width)] = true;
                offsets[cells + ":" + Math.round(cell.mapToItem(card, 0, 0).x)] = true;
            }
            widest = Math.max(widest, cells);
        }
        verify(widest > 0, "no cost pair was laid out at " + data.tag);
        compare(Object.keys(pitches).length, 1, "cost pairs use " + Object.keys(pitches).length + " different pitches at " + data.tag);
        compare(Object.keys(offsets).length, widest, "cost slot " + widest + " columns resolved to " + Object.keys(offsets).length + " offsets at " + data.tag);
        panel.destroy();
    }

    function test_no_barracks_label_is_cut_off_by_its_own_box_data() {
        return layout_data();
    }

    function test_no_barracks_label_is_cut_off_by_its_own_box(data) {
        Core.UiPreferences.uiScale = data.scale;
        var panel = make_barracks(data.w, data.h);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var labels = testCase.collect_text(view);
        verify(labels.length > 0, "the barracks view rendered no text at " + data.tag);
        for (var i = 0; i < labels.length; ++i) {
            var box = labels[i].parent;
            if (box === null || box.width <= 0 || box.height <= 0)
                continue;
            verify(labels[i].contentHeight <= box.height + 1.0, "\"" + String(labels[i].text).substring(0, 28) + "\" is " + labels[i].contentHeight + "px tall inside a " + box.height + "px box at " + data.tag);
            verify(labels[i].y + labels[i].contentHeight <= box.height + 1.0, "\"" + String(labels[i].text).substring(0, 28) + "\" ends " + Math.round(labels[i].y + labels[i].contentHeight - box.height) + "px below its " + box.height + "px box at " + data.tag);
        }
        panel.destroy();
    }

    function test_the_reserve_readout_fits_inside_the_header_data() {
        return layout_data();
    }

    function test_the_reserve_readout_fits_inside_the_header(data) {
        Core.UiPreferences.uiScale = data.scale;
        var panel = make_barracks(data.w, data.h);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var header = testCase.collect(view, "barracksHeader")[0];
        verify(header !== undefined, "the barracks header is missing");
        var labels = testCase.collect_text(header);
        var reserve = null;
        for (var i = 0; i < labels.length; ++i) {
            if (String(labels[i].text).indexOf("Reserve ") === 0)
                reserve = labels[i];
        }
        verify(reserve !== null, "the reserve readout is missing at " + data.tag);
        var top = reserve.mapToItem(header, 0, 0);
        verify(top.y >= -0.5, "the reserve readout starts above the header at " + data.tag);
        verify(top.y + reserve.contentHeight <= header.height + 0.5, "the reserve readout runs " + Math.round(top.y + reserve.contentHeight - header.height) + "px past the bottom of the header at " + data.tag);
        var progress = testCase.collect(view, "barracksProductionProgress")[0];
        verify(progress !== undefined && progress.visible, "the progress bar is missing at " + data.tag);
        var bar = progress.mapToItem(header, 0, 0);
        var overlapY = Math.min(top.y + reserve.contentHeight, bar.y + progress.height) - Math.max(top.y, bar.y);
        var overlapX = Math.min(top.x + reserve.width, bar.x + progress.width) - Math.max(top.x, bar.x);
        verify(overlapY <= 0 || overlapX <= 0, "the progress bar paints over the reserve readout at " + data.tag);
        panel.destroy();
    }

    function test_the_recruit_list_never_rests_on_a_half_row_data() {
        return layout_data();
    }

    function test_the_recruit_list_never_rests_on_a_half_row(data) {
        Core.UiPreferences.uiScale = data.scale;
        var panel = make_barracks(data.w, data.h);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var grid = testCase.collect(view, "barracksRecruitGrid")[0];
        verify(grid.height >= grid.cellHeight, "the recruit list did not reserve one whole row at " + data.tag);
        fuzzyCompare(grid.height / grid.cellHeight, Math.round(grid.height / grid.cellHeight), 0.01, "the recruit viewport exposes a partial card row at " + data.tag);
        panel.destroy();
    }

    function test_a_scrolling_list_keeps_its_bar_out_of_the_cards() {
        var panel = make_barracks(421, 184);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var grid = testCase.collect(view, "barracksRecruitGrid")[0];
        verify(grid.contentHeight > grid.height, "the 1280x720 recruit list was expected to overflow");
        var bars = testCase.collect(view, "barracksRecruitScrollBar");
        compare(bars.length, 1, "the recruit list has no scroll bar");
        var bar = bars[0];
        verify(bar.visible, "an overflowing recruit list hides its scroll bar");
        var cards = testCase.collect(view, "recruitCardName");
        for (var i = 0; i < cards.length; ++i) {
            var card = cards[i].parent.parent;
            var right = card.mapToItem(grid, card.width, 0).x;
            verify(right <= grid.width - bar.width + 0.5, "a recruit card runs under the scroll bar gutter");
        }
        panel.destroy();
    }

    function test_a_long_unit_name_elides_and_keeps_its_full_text_in_the_tooltip() {
        var panel = make_barracks(634, 224);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var labels = testCase.collect(view, "recruitCardName");
        var spear = null;
        for (var i = 0; i < labels.length; ++i) {
            if (labels[i].parent.parent.objectName === "recruitCard_spearman")
                spear = labels[i];
        }
        verify(spear !== null, "the spearman card is missing");
        compare(spear.elide, Text.ElideRight, "a unit name can overflow its reserved width");
        compare(spear.text, "Liby-Phoenician Spear", "the card dropped the full unit name");
        var card = spear.parent.parent;
        verify(card.tooltip_text.indexOf("Liby-Phoenician Spear") !== -1, "the tooltip does not carry the full unit name");
        verify(spear.width <= card.width, "the name reserve is wider than the card");
        panel.destroy();
    }

    function test_the_queue_marks_occupied_slots_apart_from_empty_ones() {
        var panel = make_barracks(634, 224);
        var view = testCase.collect(panel, "barracksProductionView")[0];
        var row = testCase.collect(view, "barracksQueueRow")[0];
        verify(row !== undefined, "the barracks queue row is missing");
        var occupied = 0;
        var empty = 0;
        for (var i = 0; i < row.children.length; ++i) {
            var slot = row.children[i];
            if (slot.occupied === undefined)
                continue;
            if (slot.occupied)
                occupied++;
            else
                empty++;
            verify(slot.width > 0 && slot.height > 0, "a queue slot collapsed");
        }
        compare(occupied, 3, "the queue did not mark one in-progress plus two queued units");
        compare(empty, 2, "the queue did not leave two empty slots");
        var progress = testCase.collect(view, "barracksProductionProgress")[0];
        verify(progress !== undefined, "the production progress bar is missing");
        verify(progress.visible, "a barracks that is recruiting hides its progress bar");
        verify(progress.value > 0 && progress.value < 1, "the progress bar does not read the build clock");
        panel.destroy();
    }
}
