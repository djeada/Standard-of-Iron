pragma Singleton
import QtQuick 2.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design

QtObject {
    id: root

    readonly property bool edge_scroll_enabled: Design.A11y.edgeScrollEnabled
    readonly property real edge_scroll_sensitivity: Design.A11y.edgeScrollSensitivity

    readonly property real edge_zone_width: Core.EdgeScroll.horizontalZone(Design.A11y.edgeScrollSensitivity, Design.A11y.uiScale)
    readonly property real edge_zone_height: Core.EdgeScroll.verticalZone(Design.A11y.edgeScrollSensitivity, Design.A11y.uiScale)

    function shortcut_for(action_id) {
        var shortcut = Core.InputBindings.shortcut_for(action_id);
        return shortcut.length > 0 ? Core.InputBindings.describe(shortcut) : qsTr("unbound");
    }

    function pan_keys() {
        var keys = [];
        var actions = ["rts.camera_pan_up", "rts.camera_pan_left", "rts.camera_pan_down", "rts.camera_pan_right"];
        for (var i = 0; i < actions.length; ++i) {
            var shortcut = Core.InputBindings.shortcut_for(actions[i]);
            if (shortcut.length > 0)
                keys.push(Core.InputBindings.describe(shortcut));
        }
        return keys.length > 0 ? keys.join(" ") : qsTr("unbound");
    }

    function orbit_keys() {
        var left = Core.InputBindings.shortcut_for("rts.camera_yaw_left");
        var right = Core.InputBindings.shortcut_for("rts.camera_yaw_right");
        if (left.length === 0 || right.length === 0)
            return qsTr("unbound");
        return Core.InputBindings.describe(left) + " " + Core.InputBindings.describe(right);
    }

    readonly property string edge_scroll_state: edge_scroll_enabled ? qsTr("on, %1 px").arg(Math.round(root.edge_zone_width)) : qsTr("off")

    readonly property var entries: [{
            "key": "edge_scroll",
            "compact": qsTr("Screen edge"),
            "name": qsTr("Edge scroll"),
            "control": qsTr("Cursor to a screen edge"),
            "detail": qsTr("Push the pointer into any edge of the screen and the camera follows. Turn it off or change how wide the band is under Settings › Accessibility."),
            "state": root.edge_scroll_state,
            "muted": !root.edge_scroll_enabled
        }, {
            "key": "keyboard_pan",
            "compact": root.pan_keys(),
            "name": qsTr("Keyboard pan"),
            "control": root.pan_keys(),
            "detail": qsTr("Pans in steps and keeps panning while held. Hold Shift to move twice as far per step."),
            "state": "",
            "muted": false
        }, {
            "key": "drag_pan",
            "compact": qsTr("Right-drag"),
            "name": qsTr("Drag pan"),
            "control": qsTr("Hold right button and drag"),
            "detail": qsTr("Drags the ground under the cursor. Edge scroll pauses while you drag so the two never fight."),
            "state": "",
            "muted": false
        }, {
            "key": "zoom",
            "compact": qsTr("Wheel"),
            "name": qsTr("Zoom"),
            "control": qsTr("Mouse wheel"),
            "detail": qsTr("Scrolls the camera closer to and further from the battle."),
            "state": "",
            "muted": false
        }, {
            "key": "orbit",
            "compact": root.orbit_keys(),
            "name": qsTr("Rotate"),
            "control": root.orbit_keys(),
            "detail": qsTr("Swings the view around the point you are looking at. Hold Shift to swing further."),
            "state": "",
            "muted": false
        }, {
            "key": "minimap",
            "compact": qsTr("Click minimap"),
            "name": qsTr("Minimap jump"),
            "control": qsTr("Click or drag the minimap"),
            "detail": qsTr("Left-click jumps the camera there; right-click sends the selection there."),
            "state": "",
            "muted": false
        }, {
            "key": "follow",
            "compact": qsTr("Top bar button"),
            "name": qsTr("Follow selection"),
            "control": qsTr("Follow button, top bar"),
            "detail": qsTr("Keeps the camera on whatever you have selected until you switch it off."),
            "state": "",
            "muted": false
        }, {
            "key": "reset",
            "compact": qsTr("Top bar button"),
            "name": qsTr("Reset camera"),
            "control": qsTr("Reset button, top bar"),
            "detail": qsTr("Returns the view to your own camp."),
            "state": "",
            "muted": false
        }]
}
