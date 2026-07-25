pragma Singleton
import QtQuick 2.15

QtObject {
    property bool enabled: true

    function hover(audioSystem) {
        if (enabled && audioSystem && audioSystem.play_ui_sound)
            audioSystem.play_ui_sound("ui_hover");
    }

    function activate(audioSystem) {
        if (enabled && audioSystem && audioSystem.play_ui_sound)
            audioSystem.play_ui_sound("ui_click");
    }

    function warning(audioSystem) {
        if (enabled && audioSystem && audioSystem.play_ui_sound)
            audioSystem.play_ui_sound("ui_warning");
    }
}
