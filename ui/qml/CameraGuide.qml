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

    function keys_for(actions, slot) {
        var keys = [];
        for (var i = 0; i < actions.length; ++i) {
            var shortcut = Core.InputBindings.shortcut_for(actions[i], slot);
            if (shortcut.length > 0)
                keys.push(Core.InputBindings.describe(shortcut));
        }
        return keys.join(" ");
    }

    readonly property var pan_actions: ["rts.camera_pan_up", "rts.camera_pan_left", "rts.camera_pan_down", "rts.camera_pan_right"]

    function pan_keys() {
        var primary = root.keys_for(root.pan_actions, Core.InputBindings.Primary);
        var alternate = root.keys_for(root.pan_actions, Core.InputBindings.Alternate);
        if (primary.length === 0 && alternate.length === 0)
            return qsTr("unbound");
        if (primary.length === 0)
            return alternate;
        if (alternate.length === 0)
            return primary;
        return qsTr("%1 or %2").arg(primary).arg(alternate);
    }

    function pair_keys(left_action, right_action) {
        var left = Core.InputBindings.shortcut_for(left_action);
        var right = Core.InputBindings.shortcut_for(right_action);
        if (left.length === 0 || right.length === 0)
            return qsTr("unbound");
        return Core.InputBindings.describe(left) + " " + Core.InputBindings.describe(right);
    }

    function rotate_keys() {
        return root.pair_keys("rts.camera_rotate_left", "rts.camera_rotate_right");
    }

    function tilt_keys() {
        return root.pair_keys("rts.camera_tilt_up", "rts.camera_tilt_down");
    }

    function zoom_keys() {
        return root.pair_keys("rts.camera_zoom_in", "rts.camera_zoom_out");
    }

    function reset_key() {
        var shortcut = Core.InputBindings.shortcut_for("rts.camera_reset");
        return shortcut.length > 0 ? Core.InputBindings.describe(shortcut) : qsTr("unbound");
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
            "detail": qsTr("Slides the view over the battlefield and keeps going while held. Either the arrows or WASD; hold Shift to cover twice the ground per step."),
            "state": "",
            "muted": false
        }, {
            "key": "drag_pan",
            "compact": qsTr("Middle-drag"),
            "name": qsTr("Drag pan"),
            "control": qsTr("Hold middle button and drag"),
            "detail": qsTr("Drags the ground under the cursor. Edge scroll pauses while you drag so the two never fight."),
            "state": "",
            "muted": false
        }, {
            "key": "zoom",
            "compact": qsTr("Wheel or %1").arg(root.zoom_keys()),
            "name": qsTr("Zoom"),
            "control": qsTr("Mouse wheel, or %1").arg(root.zoom_keys()),
            "detail": qsTr("Moves the camera closer to and further from the battle."),
            "state": "",
            "muted": false
        }, {
            "key": "rotate",
            "compact": root.rotate_keys(),
            "name": qsTr("Rotate"),
            "control": root.rotate_keys(),
            "detail": qsTr("Swings the view around the point you are looking at. Hold Shift to swing further, or hold Alt and drag with the middle button."),
            "state": "",
            "muted": false
        }, {
            "key": "tilt",
            "compact": root.tilt_keys(),
            "name": qsTr("Tilt"),
            "control": root.tilt_keys(),
            "detail": qsTr("Raises the camera towards an overhead view or lowers it towards the horizon. Hold Shift to tilt further."),
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
            "compact": root.reset_key(),
            "name": qsTr("Reset camera"),
            "control": qsTr("%1, or the Reset button in the top bar").arg(root.reset_key()),
            "detail": qsTr("Returns the view to your own camp, framed the way the battle opened."),
            "state": "",
            "muted": false
        }]
}
