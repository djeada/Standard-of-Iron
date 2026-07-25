import QtQuick 2.15
import ".." as Design

IronButton {
    id: root

    property string iconText: ""
    property string hotkey: ""
    property bool active: false
    property int queuedCount: 0
    text: iconText + (hotkey.length ? "\n" + hotkey : "")
    tone: active ? "primary" : "secondary"
    implicitWidth: 58
    implicitHeight: 54
}
