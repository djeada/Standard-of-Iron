import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "ProductionPanel"
    when: windowShown
    width: 480
    height: 640
    visible: true

    function makePanel(production, placement, playerState, panelWidth, panelHeight) {
        var panel = panelComponent.createObject(testCase, {
                "width": panelWidth !== undefined ? panelWidth : 420,
                "height": panelHeight !== undefined ? panelHeight : 600,
                "production": production !== undefined ? production : null,
                "placement": placement !== undefined ? placement : null,
                "player_state": playerState !== undefined ? playerState : null
            });
        verify(panel !== null, "the production panel was not created");
        wait(1);
        return panel;
    }

    QtObject {
        id: barracksProduction

        function has_selected_type(type) {
            return type === "barracks";
        }

        function selected_state() {
            return {
                "has_barracks": true,
                "produced_count": 3,
                "max_units": 500,
                "queue_size": 2,
                "in_progress": true,
                "production_queue": ["swordsman", "spearman"],
                "product_type": "archer",
                "manpower_available": 120,
                "build_time": 10,
                "time_remaining": 4.5,
                "nation_id": "roman_republic"
            };
        }

        function unit_info(unitType, nationId) {
            return {
                "cost": 50,
                "resource_costs": {
                    "food": 20
                },
                "build_time": 8,
                "display_name": unitType === "horse_swordsman" ? "Mounted Knight" : unitType.charAt(0).toUpperCase() + unitType.slice(1)
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

    function test_an_empty_model_assigns_nothing_undefined_to_a_bool() {
        failOnWarning(/Unable to assign \[undefined\]/);
        var panel = testCase.makePanel(null, null, null);
        wait(1);
        panel.destroy();
    }

    function test_a_panel_with_no_selection_shows_no_production_state() {
        failOnWarning(/Unable to assign \[undefined\]/);
        var panel = testCase.makePanel(null, null, null);
        wait(1);
        verify(panel.width > 0, "the panel collapsed instead of laying out");
        panel.destroy();
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

    function test_the_recruitment_reserve_is_not_called_population() {
        var panel = testCase.makePanel(null, null, null);
        var labels = testCase.collect(panel, "barracksReserveLabel").concat(testCase.collect(panel, "templeReserveLabel"));
        compare(labels.length, 2, "both reserve readouts should be present");
        for (var i = 0; i < labels.length; ++i) {
            verify(labels[i].text.indexOf("reserve") !== -1, "a building reserve did not name itself: " + labels[i].text);
            verify(labels[i].text.indexOf("Population") === -1, "a building reserve still calls itself population: " + labels[i].text);
        }
        panel.destroy();
    }

    function test_barracks_cards_keep_complete_portraits_and_snap_by_rows() {
        var panel = testCase.makePanel(barracksProduction, null, fundedPlayer, 360, 172);
        tryCompare(panel, "has_barracks_selection", true);
        wait(20);
        var grids = testCase.collect(panel, "barracksRecruitGrid");
        compare(grids.length, 1, "the dedicated barracks recruit grid is missing");
        var grid = grids[0];
        verify(grid.clip, "recruitment must clip at complete row boundaries");
        compare(grid.snapMode, GridView.SnapToRow, "recruitment does not snap by complete rows");
        verify(grid.height >= grid.cellHeight, "the barracks did not reserve one complete recruit row");
        fuzzyCompare(grid.height / grid.cellHeight, Math.round(grid.height / grid.cellHeight), 0.01, "the recruit viewport exposes a partial card row");
        var portraits = testCase.collect(panel, "recruitPortraitImage");
        verify(portraits.length >= 2, "the first recruit row did not create its portraits");
        for (var i = 0; i < portraits.length; ++i) {
            compare(portraits[i].fillMode, Image.PreserveAspectFit, "a troop portrait can still crop its source art");
            verify(portraits[i].paintedWidth <= portraits[i].width + 0.5, "portrait art overran its horizontal bounds");
            verify(portraits[i].paintedHeight <= portraits[i].height + 0.5, "portrait art overran its vertical bounds");
        }
        var cards = testCase.collect(panel, "recruitCard_archer").concat(testCase.collect(panel, "recruitCard_swordsman"));
        verify(cards.length >= 2, "the first recruit row is incomplete");
        for (var c = 0; c < cards.length; ++c) {
            var position = cards[c].mapToItem(grid, 0, 0);
            if (position.y >= 0 && position.y < grid.height)
                verify(position.y + cards[c].height <= grid.height + 0.5, cards[c].objectName + " is cut by the resting viewport");
        }
        panel.destroy();
    }

    Component {
        id: panelComponent

        ProductionPanel {
        }
    }
}
