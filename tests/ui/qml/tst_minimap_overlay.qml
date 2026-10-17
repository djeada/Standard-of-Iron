import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "MinimapOverlay"
    when: windowShown
    width: 300
    height: 300
    visible: true

    function find_all(root, name) {
        var found = [];
        var stack = [root];
        while (stack.length > 0) {
            var item = stack.pop();
            if (item.objectName === name && item.visible)
                found.push(item);
            for (var i = 0; i < item.children.length; ++i)
                stack.push(item.children[i]);
        }
        return found;
    }

    function test_live_ally_calls_are_pinned_until_they_end() {
        var overlay = overlayComponent.createObject(testCase, {
                "minimap": minimap,
                "paintedW": 200,
                "paintedH": 200
            });
        verify(overlay !== null);
        minimap.ally_markers = [{
                "id": 4,
                "kind": "attack",
                "nx": 0.25,
                "ny": 0.75,
                "pledged": false
            }, {
                "id": 5,
                "kind": "defend",
                "nx": 0.5,
                "ny": 0.5,
                "pledged": true
            }];
        compare(find_all(overlay, "allyCallMarker").length, 2, "one pin per live call");
        var pin = find_all(overlay, "allyCallMarker").filter(function (p) {
                return p.attack;
            })[0];
        compare(pin.x, overlay.mapX(0.25));
        compare(pin.y, overlay.mapY(0.75));
        minimap.event_blip(0.25, 0.75, "ally_call", "ally", 0.5);
        minimap.ally_markers = [];
        compare(find_all(overlay, "allyCallMarker").length, 0, "an ended call leaves no pin");
        overlay.destroy();
    }

    QtObject {
        id: minimap

        property var ally_markers: []
        property var landmarks: []
        property var destinations: []

        signal event_blip(real nx, real ny, string kind, string relation, real lifetime_seconds)
    }

    Component {
        id: overlayComponent

        MinimapOverlay {
        }
    }
}
