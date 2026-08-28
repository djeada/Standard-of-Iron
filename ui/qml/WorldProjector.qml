import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var camera: null
    property bool active: true
    property real topInset: 0
    property real bottomInset: 0
    property int interval: Design.A11y.reducedMotion ? 33 : 16

    readonly property bool ready: root.camera !== null && !!root.camera.project_world

    property int tick: 0

    visible: false
    enabled: false
    Accessible.ignored: true

    function project(wx, wy, wz) {
        if (!root.ready)
            return null;
        var proj = root.camera.project_world(wx, wy, wz);
        if (!proj || !proj.valid)
            return null;
        return proj;
    }

    function clamped_y(y, margin) {
        var low = root.topInset + margin;
        var high = root.height - root.bottomInset - margin;
        if (high <= low)
            return y;
        return Math.max(low, Math.min(high, y));
    }

    Timer {
        interval: root.interval
        running: root.active && root.ready
        repeat: true
        onTriggered: root.tick++
    }
}
