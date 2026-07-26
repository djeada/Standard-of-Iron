import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio

Design.IronDropdown {
    id: root

    property int text_point_size: -1
    property int text_pixel_size: -1
    property var delegate_text: function (data) {
        return data;
    }

    function resolve_delegate_text(data) {
        return (typeof delegate_text === "function") ? delegate_text(data) : data;
    }

    labelFor: root.resolve_delegate_text

    onHoveredChanged: {
        if (hovered && enabled && typeof game !== "undefined")
            UiAudio.play_hover(game.audio_system);
    }
    onActivated: {
        if (typeof game !== "undefined")
            UiAudio.play_click(game.audio_system);
    }
}
