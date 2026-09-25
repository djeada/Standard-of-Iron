import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

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
}
