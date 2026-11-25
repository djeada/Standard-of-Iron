import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "ControlGroupStrip"
    when: windowShown
    width: 400
    height: 200
    visible: true

    function test_the_strip_lists_only_groups_that_hold_units() {
        var strip = createTemporaryObject(stripComponent, testCase, {
                "sizes": [0, 3, 0, 12, 0, 0, 0, 0, 0, 0]
            });
        verify(strip.visible, "groups 1 and 3 hold units, so the strip must show");
        compare(strip.held.length, 2);
        compare(strip.held[0].group, 1);
        compare(strip.held[1].count, 12);
        verify(findChild(strip, "controlGroupChip_1") !== null);
        verify(findChild(strip, "controlGroupChip_3") !== null);
        verify(findChild(strip, "controlGroupChip_2") === null, "an empty group has no chip");
    }

    function test_an_empty_strip_hides_itself() {
        var strip = createTemporaryObject(stripComponent, testCase, {
                "sizes": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
            });
        verify(!strip.visible, "a strip with nothing in it is just clutter");
    }

    function test_the_gate_hides_the_strip_in_commander_mode() {
        var strip = createTemporaryObject(stripComponent, testCase, {
                "gate": false,
                "sizes": [0, 4, 0, 0, 0, 0, 0, 0, 0, 0]
            });
        verify(!strip.visible);
    }

    Component {
        id: stripComponent

        ControlGroupStrip {
        }
    }
}
