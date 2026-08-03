import QtQuick 2.15
import ".." as Design

// Places activity markers over a battlefield.
// Purely presentational: it is handed a list of already-grouped markers in
// screen coordinates and draws one badge per entry. Deciding which units earn a
// marker, and merging the ones doing the same job in the same place, happens in
// the engine — see App::Models::group_activity_markers.
Item {
    id: root

    // [{ activity, state, count, x, y, unitId }]
    property var markers: []
    property real iconScale: 0.8
    property real markerOpacity: 0.92

    readonly property int markerCount: root.markers ? root.markers.length : 0

    visible: root.markerCount > 0

    Repeater {
        id: badges

        model: root.markers

        delegate: Design.IronActivityIcon {
            required property var modelData

            objectName: "activityMarker"
            // The engine reports the anchor point; the badge hangs above it.
            x: modelData.x - width / 2
            y: modelData.y - height
            activity: modelData.activity !== undefined ? modelData.activity : Design.ActivityIcons.defaultActivity
            state_id: modelData.state !== undefined ? modelData.state : Design.ActivityIcons.defaultState
            count: modelData.count !== undefined ? modelData.count : 1
            iconScale: root.iconScale
            interactive: false
            opacity: root.markerOpacity

            Behavior on opacity  {
                NumberAnimation {
                    duration: Design.Motion.fast
                }
            }
        }
    }
}
