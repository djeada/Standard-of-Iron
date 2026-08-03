import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Design.IronActivityMarkerLayer {
    id: overlay

    property bool enabled: true
    property int refreshInterval: 220
    property var engine: typeof game !== 'undefined' ? game : null

    anchors.fill: parent

    function activity_source() {
        return overlay.engine && overlay.engine.activity ? overlay.engine.activity : null;
    }

    function refresh() {
        var source = overlay.activity_source();
        if (!overlay.enabled || source === null) {
            overlay.markers = [];
            return;
        }
        overlay.markers = source.markers();
    }

    onEnabledChanged: overlay.refresh()

    Timer {
        interval: overlay.refreshInterval
        repeat: true
        running: overlay.enabled
        triggeredOnStart: true

        onTriggered: overlay.refresh()
    }
}
