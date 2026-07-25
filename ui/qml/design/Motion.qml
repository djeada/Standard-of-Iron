pragma Singleton
import QtQuick 2.15

QtObject {
    property bool reducedMotion: false
    readonly property int immediate: 0
    readonly property int fast: reducedMotion ? 0 : 90
    readonly property int normal: reducedMotion ? 0 : 160
    readonly property int deliberate: reducedMotion ? 0 : 260
    readonly property int cinematic: reducedMotion ? 0 : 520
    readonly property int standardEasing: Easing.OutCubic
    readonly property int emphasizedEasing: Easing.OutQuart
}
