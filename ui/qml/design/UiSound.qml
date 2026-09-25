pragma Singleton
import QtQuick 2.15

QtObject {
    property bool enabled: true
    property var audioSystem: null

    function cue(cue_id) {
        if (enabled && audioSystem && audioSystem.play_cue)
            audioSystem.play_cue(cue_id);
    }

    function play(kind) {
        if (kind === "none")
            return;
        if (kind === "back")
            back();
        else if (kind === "confirm")
            confirm();
        else if (kind === "toggle")
            toggle();
        else
            activate();
    }

    function confirm() {
        cue("ui.confirm");
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

    function notification() {
        cue("ui.notification");
    }
}
