import QtQuick 2.15
import QtTest 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.TestSupport 1.0

TestCase {
    id: root

    name: "SelectionSummaryChurn"
    when: windowShown
    width: 420
    height: 320
    visible: true

    function unitRows(count) {
        var out = [];
        for (var i = 0; i < count; ++i)
            out.push({
                    "name": "Archer",
                    "unit_type": "archer",
                    "nation": "carthage",
                    "unit_id": "u" + i,
                    "health_ratio": 0.8,
                    "activity": "idle",
                    "activity_state": "",
                    "soldiers": 20,
                    "maxSoldiers": 22
                });
        return out;
    }

    function groupRows(count) {
        var out = [];
        for (var i = 0; i < count; ++i)
            out.push({
                    "typeKey": "archer",
                    "name": "Archer",
                    "nation": "carthage",
                    "health": 0.8,
                    "woundedCount": 0,
                    "count": 2,
                    "mixedActivity": false,
                    "soldiers": 20,
                    "maxSoldiers": 22
                });
        return out;
    }

    function applySelection(units, groups) {
        summary.model = root.unitRows(units);
        summary.unitCount = units;
        summary.groups = root.groupRows(groups);
    }

    function noise() {
        var out = [];
        var all = WarningProbe.messages();
        for (var i = 0; i < all.length; ++i) {
            if (all[i].indexOf("IronSelectionSummary") >= 0 || all[i].indexOf("invalid context") >= 0)
                out.push(all[i]);
        }
        return out;
    }

    Design.IronSelectionSummary {
        id: summary

        anchors.fill: parent
        unitCount: 0
        model: []
        groups: []
    }

    function test_the_summary_is_quiet_while_the_selection_churns() {
        WarningProbe.start();
        root.applySelection(6, 6);
        wait(80);
        root.applySelection(0, 0);
        wait(80);
        root.applySelection(1, 1);
        wait(80);
        summary.groups = [];
        wait(80);
        root.applySelection(3, 3);
        wait(80);
        root.applySelection(6, 6);
        wait(120);
        WarningProbe.stop();
        var noisy = root.noise();
        compare(noisy.length, 0, "a churning selection must not evaluate bindings against a row that has gone away — " + noisy.slice(0, 3).join(" | "));
    }

    function test_the_group_cards_rebuild_with_real_values() {
        root.applySelection(6, 6);
        wait(120);
        var groupBar = findChild(summary, "selectionGroupHealthBar_archer");
        verify(groupBar !== null, "the group cards should carry their type keys");
        compare(groupBar.value, 0.8);
    }
}
