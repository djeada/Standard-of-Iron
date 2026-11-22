import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var bearings: []

    property real viewYaw: 0
    property real unit: 1.0
    property real fontPixels: 12

    readonly property real halfSpanDegrees: 90
    readonly property real pixelsPerDegree: (width * 0.5 - root.edgeInset) / halfSpanDegrees
    readonly property real edgeInset: Math.round(14 * unit)
    readonly property color plate: Design.Theme.backgroundDeep
    readonly property color bronze: Design.Theme.accent
    readonly property color bone: Design.Theme.parchment
    readonly property color ember: Design.Theme.danger

    readonly property int captionIndex: {
        var best = -1;
        var bestOff = 9.0;
        for (var i = 0; i < bearings.length; ++i) {
            var off = Math.abs(Number(bearings[i].bearing));
            var reach = bearings[i].kind === "enemy_commander" ? 14.0 : 9.0;
            if (off < reach && off < bestOff + (bearings[i].kind === "enemy_commander" ? 5.0 : 0.0)) {
                best = i;
                bestOff = off;
            }
        }
        return best;
    }

    function wrap(degrees) {
        var d = degrees % 360;
        if (d > 180)
            d -= 360;
        if (d <= -180)
            d += 360;
        return d;
    }

    function glyph_for(kind) {
        switch (kind) {
        case "enemy_commander":
            return Design.Icons.commander;
        case "army":
            return Design.Icons.defense;
        case "rally":
            return Design.Icons.rally;
        default:
            return Design.Icons.attack;
        }
    }

    function color_for(kind) {
        switch (kind) {
        case "enemy_commander":
        case "enemy":
            return root.ember;
        case "army":
            return root.bronze;
        default:
            return root.bone;
        }
    }

    function caption_for(entry) {
        var metres = qsTr("%1 m").arg(Math.round(Number(entry.distance)));
        switch (entry.kind) {
        case "enemy_commander":
            return entry.label && entry.label.length > 0 ? entry.label + "  ·  " + metres : qsTr("Enemy commander") + "  ·  " + metres;
        case "army":
            return qsTr("Your army") + "  ·  " + metres;
        case "rally":
            return qsTr("Rally flag") + "  ·  " + metres;
        default:
            return qsTr("Enemy") + "  ·  " + metres;
        }
    }

    implicitHeight: Math.round(36 * unit)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Qt.rgba(root.plate.r, root.plate.g, root.plate.b, 0.62)
        border.width: Math.max(1, Math.round(1.5 * root.unit))
        border.color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0.6)
    }

    Item {
        id: dial

        anchors.fill: parent
        anchors.leftMargin: root.edgeInset
        anchors.rightMargin: root.edgeInset
        clip: true

        Repeater {

            model: 24

            delegate: Rectangle {
                required property int index

                readonly property real off: -root.wrap(index * 15 - root.viewYaw)
                readonly property bool major: index % 3 === 0

                visible: Math.abs(off) <= root.halfSpanDegrees
                x: dial.width * 0.5 + off * root.pixelsPerDegree - width * 0.5
                y: major ? dial.height * 0.5 - height * 0.5 : dial.height - height - Math.round(5 * root.unit)
                width: Math.max(2, Math.round(2 * root.unit))
                height: Math.round((major ? 14 : 8) * root.unit)
                radius: width / 2
                color: Qt.rgba(root.bone.r, root.bone.g, root.bone.b, major ? 0.6 : 0.36)
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: -Math.round(5 * root.unit)
        width: Math.round(10 * root.unit)
        height: width
        rotation: 45
        color: Design.Theme.focus
    }

    Repeater {
        model: root.bearings

        delegate: Item {
            required property var modelData
            required property int index

            readonly property real off: Number(modelData.bearing)
            readonly property bool beyond: Math.abs(off) > root.halfSpanDegrees
            readonly property real shown: Math.max(-root.halfSpanDegrees, Math.min(root.halfSpanDegrees, off))
            readonly property bool chief: modelData.kind === "enemy_commander"

            objectName: "rpgBearingMark"

            x: root.width * 0.5 + shown * root.pixelsPerDegree - width * 0.5
            anchors.verticalCenter: parent.verticalCenter
            width: Math.round((chief ? 34 : 27) * root.unit)
            height: width
            z: chief ? 3 : (modelData.kind === "enemy" ? 2 : 1)
            opacity: beyond ? 0.62 : 1.0

            Behavior on x  {
                enabled: !beyond && !Design.Motion.reducedMotion

                NumberAnimation {
                    duration: 40
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: width
                radius: width / 2
                color: Qt.rgba(root.plate.r, root.plate.g, root.plate.b, 0.9)
                border.width: Math.max(1, Math.round((chief ? 2 : 1) * root.unit))
                border.color: root.color_for(modelData.kind)
            }

            Text {
                anchors.centerIn: parent
                text: root.glyph_for(modelData.kind)
                color: root.color_for(modelData.kind)
                font.family: Design.Typography.family
                font.pixelSize: Math.round(parent.width * 0.62)
            }

            Text {
                visible: beyond
                anchors.verticalCenter: parent.verticalCenter
                x: off < 0 ? -width - Math.round(1 * root.unit) : parent.width + Math.round(1 * root.unit)
                text: off < 0 ? "‹" : "›"
                color: root.color_for(modelData.kind)
                font.family: Design.Typography.family
                font.pixelSize: Math.round(parent.width * 0.8)
                font.weight: Design.Typography.bold
            }
        }
    }

    Item {
        id: caption

        objectName: "rpgBearingCaption"

        readonly property var entry: root.captionIndex >= 0 && root.captionIndex < root.bearings.length ? root.bearings[root.captionIndex] : null

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.bottom
        anchors.topMargin: Math.round(6 * root.unit)
        width: captionLabel.implicitWidth + Math.round(18 * root.unit)
        height: captionLabel.implicitHeight + Math.round(6 * root.unit)
        opacity: entry !== null ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.fast
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: Design.Metrics.radiusSmall
            color: Qt.rgba(root.plate.r, root.plate.g, root.plate.b, 0.58)
        }

        Text {
            id: captionLabel

            objectName: "rpgBearingCaptionLabel"

            anchors.centerIn: parent
            text: caption.entry !== null ? root.caption_for(caption.entry) : ""
            color: caption.entry !== null ? Qt.lighter(root.color_for(caption.entry.kind), 1.25) : root.bone
            font.family: Design.Typography.family
            font.pixelSize: root.fontPixels
            font.weight: Design.Typography.bold
            font.letterSpacing: Design.Typography.trackingWide
            font.capitalization: Font.AllUppercase
        }
    }
}
