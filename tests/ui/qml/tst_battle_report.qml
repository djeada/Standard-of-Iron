import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0 as Core
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "BattleReport"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    function init() {
        Core.UiPreferences.reset_to_defaults();
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function army(name, values) {
        var entry = {
            "name": name,
            "accent": "#c9a227",
            "factionId": "roman_republic",
            "factionName": "Roman Republic",
            "isLocal": false,
            "isWinner": false,
            "kills": 0,
            "losses": 0,
            "trained": 0,
            "villages": 0,
            "score": 0
        };
        for (var key in values)
            entry[key] = values[key];
        return entry;
    }

    function modestField() {
        return [testCase.army("Scipio", {
                    "isLocal": true,
                    "isWinner": true,
                    "kills": 148,
                    "losses": 62,
                    "trained": 96,
                    "villages": 7,
                    "score": 19260
                }), testCase.army("Hasdrubal", {
                    "kills": 62,
                    "losses": 148,
                    "trained": 71,
                    "villages": 2,
                    "score": 7910
                })];
    }

    function makeReport(w, h, armies) {
        var host = hostComponent.createObject(testCase, {
                "width": w,
                "height": h
            });
        verify(host !== null, "the report host was not created");
        host.report.armies = armies !== undefined ? armies : testCase.modestField();
        wait(1);
        return host;
    }

    function collect(item, objectName, found) {
        var out = found !== undefined ? found : [];
        if (item === null || item === undefined)
            return out;
        var kids = item.children;
        for (var i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === objectName)
                out.push(kids[i]);
            testCase.collect(kids[i], objectName, out);
        }
        return out;
    }

    function rectIn(root, item) {
        var topLeft = root.mapFromItem(item, 0, 0);
        return {
            "left": topLeft.x,
            "top": topLeft.y,
            "right": topLeft.x + item.width,
            "bottom": topLeft.y + item.height
        };
    }

    function test_every_army_gets_a_row() {
        var host = testCase.makeReport(1280, 720);
        compare(testCase.collect(host.report, "battleReportRow").length, 2);
        host.destroy();
    }

    function test_a_towering_score_stays_inside_its_cell() {
        var armies = testCase.modestField();
        armies[0].score = 1284000;
        armies[1].score = 19260;
        var host = testCase.makeReport(1280, 720, armies);
        var scores = testCase.collect(host.report, "battleReportScore");
        compare(scores.length, 2);
        for (var i = 0; i < scores.length; ++i)
            verify(scores[i].paintedWidth <= scores[i].width + 1, "the score spilled out of its cell: " + scores[i].text);
        host.destroy();
    }

    function test_a_towering_tally_is_written_in_arabic() {
        var armies = testCase.modestField();
        armies[0].score = 1284000;
        var host = testCase.makeReport(1280, 720, armies);
        var scores = testCase.collect(host.report, "battleReportScore");
        compare(scores[0].text, Numerals.grouped(1284000));
        verify(scores[0].text.indexOf("M") === -1, "a six figure score was spelled out in M glyphs");
        host.destroy();
    }

    function test_a_column_speaks_one_numeral_system() {
        var armies = testCase.modestField();
        armies[0].kills = 9000;
        var host = testCase.makeReport(1280, 720, armies);
        var rows = testCase.collect(host.report, "battleReportRow");
        compare(rows.length, 2);
        var leader = testCase.collect(rows[0], "battleReportFigure");
        var trailer = testCase.collect(rows[1], "battleReportFigure");
        compare(leader[0].text, Numerals.grouped(9000));
        compare(trailer[0].text, Numerals.grouped(62), "the same column mixed roman and arabic figures");
        host.destroy();
    }

    function test_the_duration_keeps_its_roman_chrome() {
        var host = testCase.makeReport(1280, 720);
        host.report.durationText = Numerals.span(1834);
        compare(host.report.durationText, Numerals.span(1834));
        verify(Numerals.span(1834).indexOf("X") !== -1, "the duration lost its roman styling");
        host.destroy();
    }

    function test_a_measured_tally_is_written_in_arabic() {
        var host = testCase.makeReport(1280, 720);
        var rows = testCase.collect(host.report, "battleReportRow");
        var figures = testCase.collect(rows[0], "battleReportFigure");
        compare(figures[0].text, Numerals.grouped(148));
        verify(figures[0].text.indexOf("C") === -1, "a kill count was spelled out in roman glyphs");
        host.destroy();
    }

    function test_a_zero_tally_reads_as_a_digit_not_a_letter() {
        var armies = testCase.modestField();
        armies[0].kills = 0;
        armies[1].kills = 0;
        var host = testCase.makeReport(1280, 720, armies);
        var rows = testCase.collect(host.report, "battleReportRow");
        var figures = testCase.collect(rows[0], "battleReportFigure");
        compare(figures[0].text, "0", "a zero tally was printed as the roman nulla glyph");
        host.destroy();
    }

    function test_a_four_figure_score_is_readable() {
        var armies = testCase.modestField();
        armies[0].score = 2000;
        armies[1].score = 700;
        var host = testCase.makeReport(1280, 720, armies);
        var scores = testCase.collect(host.report, "battleReportScore");
        compare(scores[0].text, Numerals.grouped(2000));
        compare(scores[1].text, Numerals.grouped(700));
        verify(scores[0].text !== "MM", "a score was printed as roman numerals");
        host.destroy();
    }

    function test_every_figure_stays_inside_its_cell_data() {
        return [{
                "tag": "modest",
                "kills": 148,
                "score": 19260
            }, {
                "tag": "ceiling",
                "kills": 3999,
                "score": 3999
            }, {
                "tag": "beyond",
                "kills": 48000,
                "score": 9284000
            }];
    }

    function test_every_figure_stays_inside_its_cell(data) {
        var armies = testCase.modestField();
        armies[0].kills = data.kills;
        armies[0].score = data.score;
        var host = testCase.makeReport(1280, 720, armies);
        var cells = testCase.collect(host.report, "battleReportFigure").concat(testCase.collect(host.report, "battleReportScore"));
        verify(cells.length > 0);
        for (var i = 0; i < cells.length; ++i)
            verify(cells[i].paintedWidth <= cells[i].width + 1, data.tag + ": " + cells[i].text + " spilled out of its cell");
        host.destroy();
    }

    function test_a_long_army_name_elides_instead_of_pushing_the_figures_out() {
        var armies = testCase.modestField();
        armies[0].name = "Publius Cornelius Scipio Africanus of the Second Legion";
        var host = testCase.makeReport(1024, 720, armies);
        var rows = testCase.collect(host.report, "battleReportRow");
        var figures = testCase.collect(rows[0], "battleReportFigure");
        var sheet = findChild(host.report, "battleReportSheet");
        for (var i = 0; i < figures.length; ++i) {
            var r = testCase.rectIn(sheet, figures[i]);
            verify(r.right <= sheet.width + 1, "a figure was pushed past the sheet edge");
        }
        host.destroy();
    }

    function test_the_report_never_overflows_the_viewport_data() {
        var sizes = [{
                "w": 1920,
                "h": 1080
            }, {
                "w": 1280,
                "h": 720
            }, {
                "w": 1024,
                "h": 768
            }, {
                "w": 800,
                "h": 600
            }, {
                "w": 640,
                "h": 480
            }];
        var scales = [1.0, 1.5, 2.0];
        var rows = [];
        for (var i = 0; i < sizes.length; ++i) {
            for (var s = 0; s < scales.length; ++s) {
                rows.push({
                        "tag": sizes[i].w + "x" + sizes[i].h + "@" + scales[s],
                        "w": sizes[i].w,
                        "h": sizes[i].h,
                        "scale": scales[s]
                    });
            }
        }
        return rows;
    }

    function test_the_report_never_overflows_the_viewport(data) {
        Core.UiPreferences.uiScale = data.scale;
        var crowd = testCase.modestField().concat([testCase.army("Vercingetorix", {
                        "kills": 12,
                        "score": 2000
                    }), testCase.army("Mago", {
                        "kills": 3,
                        "score": 800
                    })]);
        var host = testCase.makeReport(data.w, data.h, crowd);
        var sheet = findChild(host.report, "battleReportSheet");
        verify(sheet !== null, "the report sheet is missing");
        var r = testCase.rectIn(host.report, sheet);
        var tolerance = 1.0;
        verify(r.left >= -tolerance, "the sheet overflows the left edge at " + data.tag);
        verify(r.top >= -tolerance, "the sheet overflows the top edge at " + data.tag);
        verify(r.right <= host.report.width + tolerance, "the sheet overflows the right edge at " + data.tag);
        verify(r.bottom <= host.report.height + tolerance, "the sheet overflows the bottom edge at " + data.tag);
        host.destroy();
    }

    function test_a_narrow_report_keeps_only_the_essential_columns() {
        var host = testCase.makeReport(640, 720);
        verify(host.report.compact, "a 640 wide report should fold to its essential columns");
        compare(host.report.visibleColumns.length, 2);
        var rows = testCase.collect(host.report, "battleReportRow");
        compare(testCase.collect(rows[0], "battleReportFigure").length, 2);
        host.destroy();
    }

    function test_a_wide_report_shows_every_column() {
        var host = testCase.makeReport(1280, 720);
        verify(!host.report.compact);
        compare(host.report.visibleColumns.length, 4);
        host.destroy();
    }

    function test_the_tiles_report_the_local_army() {
        var host = testCase.makeReport(1280, 900);
        verify(host.report.showTiles, "a roomy report should show the commander tiles");
        var tiles = testCase.collect(host.report, "battleReportTile");
        compare(tiles.length, 4);
        host.destroy();
    }

    function test_a_cramped_report_drops_the_tiles_before_the_table() {
        var host = testCase.makeReport(1280, 420);
        verify(!host.report.showTiles, "the tiles must give way when there is no room");
        compare(testCase.collect(host.report, "battleReportRow").length, 2);
        host.destroy();
    }

    function test_a_field_without_a_local_army_still_reports() {
        var armies = testCase.modestField();
        armies[0].isLocal = false;
        var host = testCase.makeReport(1280, 900, armies);
        compare(host.report.localArmy, null);
        verify(!host.report.showTiles);
        compare(testCase.collect(host.report, "battleReportRow").length, 2);
        host.destroy();
    }

    function test_an_empty_field_does_not_break_the_report() {
        var host = testCase.makeReport(1280, 720, []);
        compare(testCase.collect(host.report, "battleReportRow").length, 0);
        var sheet = findChild(host.report, "battleReportSheet");
        verify(sheet !== null);
        host.destroy();
    }

    function test_the_actions_report_the_players_choice() {
        var host = testCase.makeReport(1280, 720);
        var dismissed = spyComponent.createObject(testCase, {
                "target": host.report,
                "signalName": "dismissed"
            });
        var menu = spyComponent.createObject(testCase, {
                "target": host.report,
                "signalName": "menuRequested"
            });
        host.report.dismissed();
        host.report.menuRequested();
        compare(dismissed.count, 1);
        compare(menu.count, 1);
        dismissed.destroy();
        menu.destroy();
        host.destroy();
    }

    function test_the_buttons_answer_a_real_click() {
        var host = testCase.makeReport(1280, 720);
        var dismissed = spyComponent.createObject(testCase, {
                "target": host.report,
                "signalName": "dismissed"
            });
        var menu = spyComponent.createObject(testCase, {
                "target": host.report,
                "signalName": "menuRequested"
            });
        var close = findChild(host.report, "battleReportClose");
        var toMenu = findChild(host.report, "battleReportMenu");
        verify(close !== null && toMenu !== null, "the report actions are missing");
        mouseClick(close);
        mouseClick(toMenu);
        compare(dismissed.count, 1, "the scrim swallowed the close click");
        compare(menu.count, 1, "the scrim swallowed the menu click");
        dismissed.destroy();
        menu.destroy();
        host.destroy();
    }

    function test_a_click_on_the_scrim_does_not_reach_the_battlefield() {
        var host = testCase.makeReport(1280, 720);
        var below = clickCatcher.createObject(host, {
                "anchors.fill": undefined
            });
        below.width = host.width;
        below.height = host.height;
        below.z = -1;
        mouseClick(host, 40, 40);
        compare(below.hits, 0, "a click on the report reached the battlefield behind it");
        below.destroy();
        host.destroy();
    }

    function test_escape_closes_the_report() {
        var host = testCase.makeReport(1280, 720);
        var spy = spyComponent.createObject(testCase, {
                "target": host.report,
                "signalName": "dismissed"
            });
        host.report.forceActiveFocus();
        verify(host.report.activeFocus, "the report never took the keyboard");
        keyClick(Qt.Key_Escape);
        compare(spy.count, 1, "Esc did not close the report");
        spy.destroy();
        host.destroy();
    }

    function test_the_winner_and_the_routed_read_differently() {
        var armies = testCase.modestField();
        verify(host_verdict(armies[0]) !== host_verdict(armies[1]));
    }

    function host_verdict(army) {
        var host = testCase.makeReport(1280, 720, [army]);
        var text = host.report.verdict(army);
        host.destroy();
        return text;
    }

    Component {
        id: hostComponent

        Item {
            property alias report: reportLayout

            BattleReportLayout {
                id: reportLayout

                anchors.fill: parent
                outcome: "victory"
                headline: "Victory Secured"
                subtitle: "Enemy command has fallen."
                missionName: "FOREST CROSSING"
                durationText: "XXX m"
            }
        }
    }

    Component {
        id: clickCatcher

        MouseArea {
            property int hits: 0

            onClicked: hits += 1
        }
    }

    Component {
        id: spyComponent

        SignalSpy {
        }
    }
}
