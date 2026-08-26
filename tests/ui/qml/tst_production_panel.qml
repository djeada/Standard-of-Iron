import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0 as Core
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "ProductionPanel"
    when: windowShown
    width: 480
    height: 640
    visible: true

    function makePanel(production, placement, playerState) {
        var panel = panelComponent.createObject(testCase, {
                "width": 420,
                "height": 600,
                "production": production !== undefined ? production : null,
                "placement": placement !== undefined ? placement : null,
                "player_state": playerState !== undefined ? playerState : null
            });
        verify(panel !== null, "the production panel was not created");
        wait(1);
        return panel;
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

    Component {
        id: panelComponent

        ProductionPanel {
        }
    }
}
