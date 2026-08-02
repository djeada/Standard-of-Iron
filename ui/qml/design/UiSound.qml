pragma Singleton
import QtQuick 2.15

QtObject {
    property bool enabled: true
    property var audioSystem: null

    function cue(cue_id) {
        if (enabled && audioSystem && audioSystem.play_cue)
            audioSystem.play_cue(cue_id);
    }

    function hover() {
        cue("ui.hover");
    }

    function activate() {
        cue("ui.click");
    }

    function back() {
        cue("ui.back");
    }

    function tabSwitch() {
        cue("ui.tab_switch");
    }

    function panelOpen() {
        cue("ui.panel_open");
    }

    function panelClose() {
        cue("ui.panel_close");
    }

    function toggle() {
        cue("ui.toggle");
    }

    function warning() {
        cue("ui.error");
    }
}
