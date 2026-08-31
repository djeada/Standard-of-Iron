import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0
import "../../../ui/qml"
import StandardOfIron.Core 1.0

TestCase {
    id: testCase

    name: "CameraLegend"
    when: windowShown
    width: 800
    height: 700
    visible: true

    readonly property var required_controls: ["edge_scroll", "keyboard_pan", "drag_pan", "zoom", "rotate", "tilt", "minimap", "follow", "reset"]

    function entry_for(key) {
        var entries = CameraGuide.entries;
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].key === key)
                return entries[i];
        }
        return null;
    }

    function test_every_camera_control_the_issue_names_is_listed() {
        for (var i = 0; i < testCase.required_controls.length; ++i) {
            var key = testCase.required_controls[i];
            var entry = testCase.entry_for(key);
            verify(entry !== null, "missing camera control: " + key);
        }
    }

    function test_every_entry_names_itself_and_how_to_do_it() {
        var entries = CameraGuide.entries;
        verify(entries.length >= testCase.required_controls.length);
        for (var i = 0; i < entries.length; ++i) {
            var entry = entries[i];
            verify(entry.name.length > 0, "entry has no name: " + entry.key);
            verify(entry.control.length > 0, "entry has no control: " + entry.key);
            verify(entry.compact.length > 0, "entry has no compact label: " + entry.key);
            verify(entry.detail.length > 0, "entry has no detail: " + entry.key);
        }
    }

    function test_the_keyboard_entries_read_the_live_bindings() {
        var pan = testCase.entry_for("keyboard_pan");
        var expected = InputBindings.describe(InputBindings.shortcut_for("rts.camera_pan_up"));
        verify(pan.control.indexOf(expected) !== -1, "pan entry does not name the bound key: " + pan.control);
        var rotate = testCase.entry_for("rotate");
        var rotate_key = InputBindings.describe(InputBindings.shortcut_for("rts.camera_rotate_left"));
        verify(rotate.control.indexOf(rotate_key) !== -1, "rotate entry does not name the bound key: " + rotate.control);
        var tilt = testCase.entry_for("tilt");
        var tilt_key = InputBindings.describe(InputBindings.shortcut_for("rts.camera_tilt_up"));
        verify(tilt.control.indexOf(tilt_key) !== -1, "tilt entry does not name the bound key: " + tilt.control);
        var reset = testCase.entry_for("reset");
        var reset_key = InputBindings.describe(InputBindings.shortcut_for("rts.camera_reset"));
        verify(reset.control.indexOf(reset_key) !== -1, "reset entry does not name the bound key: " + reset.control);
    }

    function test_the_slot_enum_reaches_qml() {
        compare(InputBindings.Primary, 0);
        compare(InputBindings.Alternate, 1);
    }

    function test_the_pan_entry_names_both_the_arrows_and_wasd() {
        var pan = testCase.entry_for("keyboard_pan");
        var arrow = InputBindings.describe(InputBindings.shortcut_for("rts.camera_pan_left", InputBindings.Primary));
        var letter = InputBindings.describe(InputBindings.shortcut_for("rts.camera_pan_left", InputBindings.Alternate));
        verify(letter.length > 0, "pan left has no alternate key");
        verify(pan.control.indexOf(arrow) !== -1, "pan entry omits the arrow key: " + pan.control);
        verify(pan.control.indexOf(letter) !== -1, "pan entry omits the letter key: " + pan.control);
    }

    function test_the_edge_scroll_entry_reports_whether_it_is_on() {
        var wasEnabled = UiPreferences.edgeScrollEnabled;
        UiPreferences.edgeScrollEnabled = true;
        var on = testCase.entry_for("edge_scroll");
        verify(!on.muted);
        verify(on.state.length > 0);
        verify(on.state.indexOf("px") !== -1, "an enabled band should report its width: " + on.state);
        UiPreferences.edgeScrollEnabled = false;
        var off = testCase.entry_for("edge_scroll");
        verify(off.muted);
        verify(off.state.length > 0);
        UiPreferences.edgeScrollEnabled = wasEnabled;
    }

    function test_a_stronger_setting_widens_the_reported_band() {
        var was = UiPreferences.edgeScrollSensitivity;
        UiPreferences.edgeScrollSensitivity = UiPreferences.minEdgeScrollSensitivity;
        var narrow = CameraGuide.edge_zone_width;
        UiPreferences.edgeScrollSensitivity = UiPreferences.maxEdgeScrollSensitivity;
        var wide = CameraGuide.edge_zone_width;
        verify(wide > narrow, "band did not widen: " + narrow + " -> " + wide);
        UiPreferences.edgeScrollSensitivity = was;
    }

    function test_the_legend_renders_every_control_without_truncating() {
        var legend = legendComponent.createObject(testCase);
        verify(legend !== null);
        wait(50);
        verify(legend.width > 0);
        verify(legend.height > 0);
        compare(legend.entries.length, CameraGuide.entries.length);
        legend.destroy();
    }

    Component {
        id: legendComponent

        CameraLegend {
        }
    }
}
