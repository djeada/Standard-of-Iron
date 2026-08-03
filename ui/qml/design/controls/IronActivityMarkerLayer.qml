import QtQuick 2.15
import ".." as Design

Item {
    id: root

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
