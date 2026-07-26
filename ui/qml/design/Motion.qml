pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property bool reducedMotion: A11y.reducedMotion

    readonly property int immediate: 0
    readonly property int fast: reducedMotion ? 0 : 90
    readonly property int normal: reducedMotion ? 0 : 160
    readonly property int deliberate: reducedMotion ? 0 : 260
    readonly property int cinematic: reducedMotion ? 0 : 520

    readonly property int standardEasing: Easing.OutCubic
    readonly property int emphasizedEasing: Easing.OutQuart
    readonly property int exitEasing: Easing.InCubic

    readonly property bool allowAmbientLoops: !reducedMotion

    function dwellFor(priority) {
        switch (priority) {
        case "critical":
            return 8000;
        case "urgent":
            return 6000;
        case "info":
            return 4000;
        default:
            return 3000;
        }
    }
}
