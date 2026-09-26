import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0
import "../../../ui/qml"

// HUDBottom reads the selection from game.selected_units_model. In the game
// that model refreshes in a C++ slot on GameEngine::selected_units_changed,
// and Qt runs QML handlers on a signal before C++ slots on the same signal, so
// the HUD's own onSelected_units_changed always reads the previous selection.
// The fake below replays that order: the engine signal first, then the model
// updates and announces it. The SELECTION zone must follow the model, not the
// stale read.
TestCase {
    id: testCase

    name: "HudBottomSelection"
    when: windowShown
    width: 1280
    height: Metrics.bottomBarHeight(720, false)
    visible: true

    property var panel: null

    QtObject {
        id: game

        signal selected_units_changed

        property var orders: ({
                "action_states": function () {
                    return ({});
                }
            })
        property var activity: null
        property var production: null
        property var placement: null
        property var commander: null
        property var tutorial: null
        property var selected_player_state: null

        property QtObject selected_units_model: QtObject {
            property int rows: 0
            property var groups: []

            signal modelReset
            signal dataChanged

            function rowCount() {
                return rows;
            }

            function grouped_by_type() {
                return groups;
            }
        }

        function select(count, groups) {
            selected_units_changed();
            selected_units_model.rows = count;
            selected_units_model.groups = groups;
            selected_units_model.modelReset();
        }
    }

    function troops(count, health) {
        return [{
                "typeKey": "swordsman",
                "name": "Swordsman",
                "nation": "carthage",
                "count": count,
                "woundedCount": 0,
                "soldiers": 0,
                "maxSoldiers": 0,
                "health": health,
                "stamina": 1,
                "canRun": true
            }];
    }

    function collect(item, predicate, found) {
        var out = found || [];
        if (!item)
            return out;
        var kids = item.children;
        for (var i = 0; i < kids.length; ++i) {
            if (predicate(kids[i]))
                out.push(kids[i]);
            collect(kids[i], predicate, out);
        }
        return out;
    }

    function selectionZone() {
        var found = collect(panel, function (item) {
                return item.objectName === "selectionZone";
            });
        compare(found.length, 1, "expected one selection zone");
        return found[0];
    }

    function init() {
        Core.UiPreferences.reset_to_defaults();
        game.selected_units_model.rows = 0;
        game.selected_units_model.groups = [];
        panel = hudComponent.createObject(testCase);
        verify(panel !== null, "the bottom HUD was not created");
        wait(1);
    }

    function cleanup() {
        if (panel) {
            panel.destroy();
            panel = null;
        }
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function test_selection_zone_shows_troops_once_the_model_refreshes() {
        compare(selectionZone().unitCount, 0);
        verify(selectionZone().empty);
        game.select(13, troops(13, 1));
        compare(panel.selection_count, 13, "the HUD kept the selection it read before the model refreshed");
        compare(selectionZone().unitCount, 13);
        verify(!selectionZone().empty, "the SELECTION zone still shows its empty prompt");
        compare(selectionZone().groups.length, 1);
    }

    function test_selection_zone_follows_model_data_updates() {
        game.select(13, troops(13, 1));
        compare(panel.selection_groups[0].health, 1);
        game.selected_units_model.groups = troops(13, 0.25);
        game.selected_units_model.dataChanged();
        compare(panel.selection_groups[0].health, 0.25, "health and activity in the SELECTION zone went stale");
    }

    Component {
        id: hudComponent

        HUDBottom {
            anchors.fill: parent
        }
    }
}
