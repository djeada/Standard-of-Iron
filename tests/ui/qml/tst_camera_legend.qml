import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "CameraLegend"
    when: windowShown
    width: 800
    height: 700
    visible: true

    readonly property var required_controls: ["edge_scroll", "keyboard_pan", "drag_pan", "zoom", "minimap", "follow", "reset"]

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
        var rotate = testCase.entry_for("orbit");
        var rotate_key = InputBindings.describe(InputBindings.shortcut_for("rts.camera_yaw_left"));
        verify(rotate.control.indexOf(rotate_key) !== -1, "rotate entry does not name the bound key: " + rotate.control);
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
