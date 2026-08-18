import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: faceRoot

    property Item anchorSource: null

    property bool talking: false

    property string pose: ""

    property color accent: "#c8a061"

    readonly property bool anchored: anchorSource !== null && anchorSource.faceValid

    property real headScale: 1.3

    readonly property real headRadius: anchorSource ? anchorSource.faceRadius * anchorSource.height * headScale : 0

    readonly property real squashX: anchorSource ? Math.sqrt(Math.max(0.0, 1.0 - (anchorSource.faceTurn * anchorSource.faceTurn))) : 1
    readonly property real squashY: anchorSource ? Math.sqrt(Math.max(0.0, 1.0 - (anchorSource.faceTilt * anchorSource.faceTilt))) : 1

    readonly property real facing: anchorSource ? anchorSource.faceFacing : 1

    readonly property bool animated: !Design.A11y.reducedMotion
    readonly property bool ambient: Design.Motion.allowAmbientLoops && faceRoot.animated

    property real blink: 0
    property real mouthOpen: 0
    property real mouthWide: 0.4
    property real gazeX: 0
    property real gazeY: 0

    readonly property real span: 2.6

    width: headRadius * span
    height: width
    x: (anchorSource ? anchorSource.faceX * anchorSource.width : 0) - (width / 2)
    y: (anchorSource ? anchorSource.faceY * anchorSource.height : 0) - (height / 2)

    visible: anchored && headRadius > 2 && facing > -0.05
    opacity: Math.max(0.0, Math.min(1.0, (facing - 0.02) / 0.30))

    transform: [
        Scale {
            origin.x: faceRoot.width / 2
            origin.y: faceRoot.height / 2
            xScale: Math.max(0.05, faceRoot.squashX)
            yScale: Math.max(0.05, faceRoot.squashY)
        },
        Rotation {
            origin.x: faceRoot.width / 2
            origin.y: faceRoot.height / 2
            angle: faceRoot.anchorSource ? faceRoot.anchorSource.faceRoll : 0
        }
    ]

    onBlinkChanged: canvas.requestPaint()
    onMouthOpenChanged: canvas.requestPaint()
    onMouthWideChanged: canvas.requestPaint()
    onGazeXChanged: canvas.requestPaint()
    onGazeYChanged: canvas.requestPaint()
    onPoseChanged: canvas.requestPaint()
    onAccentChanged: canvas.requestPaint()
    onTalkingChanged: {
        if (!faceRoot.talking) {
            mouthTarget.stop();
            mouthOpenTo.stop();
            mouthWideTo.stop();
            faceRoot.mouthOpen = 0;
            faceRoot.mouthWide = 0.4;
        }
    }

    Timer {
        id: blinkClock

        interval: 2200 + Math.random() * 3600
        repeat: true
        running: faceRoot.ambient && faceRoot.visible
        onTriggered: {
            interval = 2200 + Math.random() * 3600;
            blinkRun.restart();
        }
    }

    SequentialAnimation {
        id: blinkRun

        NumberAnimation {
            target: faceRoot
            property: "blink"
            to: 1
            duration: 70
            easing.type: Easing.InQuad
        }

        NumberAnimation {
            target: faceRoot
            property: "blink"
            to: 0
            duration: 130
            easing.type: Easing.OutQuad
        }
    }

    Timer {
        id: mouthTarget

        readonly property var apertures: [0.18, 0.72, 0.34, 0.95, 0.10, 0.58, 0.26, 0.80, 0.42, 0.14]
        readonly property var widths: [0.30, 0.75, 0.45, 0.25, 0.55, 0.85, 0.35, 0.60, 0.20, 0.50]
        property int step: 0

        interval: 78 + Math.random() * 62
        repeat: true
        running: faceRoot.talking && faceRoot.animated && faceRoot.visible
        onTriggered: {
            interval = 78 + Math.random() * 62;
            step = (step + 1) % apertures.length;
            mouthOpenTo.to = apertures[step];
            mouthWideTo.to = widths[step];
            mouthOpenTo.restart();
            mouthWideTo.restart();
        }
    }

    NumberAnimation {
        id: mouthOpenTo

        target: faceRoot
        property: "mouthOpen"
        duration: 64
        easing.type: Easing.OutQuad
    }

    NumberAnimation {
        id: mouthWideTo

        target: faceRoot
        property: "mouthWide"
        duration: 80
        easing.type: Easing.OutQuad
    }

    Timer {
        interval: 1400 + Math.random() * 2600
        repeat: true
        running: faceRoot.ambient && faceRoot.visible
        onTriggered: {
            interval = 1400 + Math.random() * 2600;
            gazeXTo.to = (Math.random() - 0.5) * 0.16;
            gazeYTo.to = (Math.random() - 0.5) * 0.09;
            gazeXTo.restart();
            gazeYTo.restart();
        }
    }

    NumberAnimation {
        id: gazeXTo

        target: faceRoot
        property: "gazeX"
        duration: 130
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: gazeYTo

        target: faceRoot
        property: "gazeY"
        duration: 130
        easing.type: Easing.OutCubic
    }

    Canvas {
        id: canvas

        anchors.fill: parent

        renderStrategy: Canvas.Immediate
        renderTarget: Canvas.Image
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            if (faceRoot.width <= 0)
                return;
            var unit = faceRoot.width / faceRoot.span;
            ctx.translate(faceRoot.width / 2, faceRoot.height / 2);
            ctx.scale(unit, unit);
            ctx.lineJoin = "round";
            ctx.lineCap = "round";
            var cynical = faceRoot.pose !== "dismissive";
            var lineInk = Qt.rgba(0.11, 0.07, 0.04, 0.95);
            var softInk = Qt.rgba(0.17, 0.10, 0.06, 0.60);
            var scleraTone = Qt.rgba(0.86, 0.79, 0.66, 0.94);
            var irisTone = Qt.rgba(0.27, 0.18, 0.10, 1.0);
            var mouthDark = Qt.rgba(0.11, 0.05, 0.04, 0.96);
            var lipTone = Qt.rgba(0.47, 0.25, 0.19, 0.90);
            var socket = ctx.createRadialGradient(0, -0.06, 0.05, 0, -0.06, 0.95);
            socket.addColorStop(0.0, Qt.rgba(0.26, 0.16, 0.09, 0.42));
            socket.addColorStop(0.62, Qt.rgba(0.26, 0.16, 0.09, 0.20));
            socket.addColorStop(1.0, Qt.rgba(0.26, 0.16, 0.09, 0.0));
            ctx.fillStyle = socket;
            ctx.beginPath();
            ctx.ellipse(-0.95, -0.85, 1.90, 1.70);
            ctx.fill();
            var eyeY = -0.02;
            var eyeDx = 0.40;
            var lidOpen = 1.0 - faceRoot.blink;
            for (var side = -1; side <= 1; side += 2) {
                var ex = side * eyeDx;
                var openH = 0.150 * lidOpen;
                if (openH > 0.006) {
                    ctx.fillStyle = scleraTone;
                    ctx.beginPath();
                    ctx.ellipse(ex - 0.215, eyeY - openH, 0.43, openH * 2);
                    ctx.fill();
                    ctx.save();
                    ctx.beginPath();
                    ctx.ellipse(ex - 0.215, eyeY - openH, 0.43, openH * 2);
                    ctx.clip();
                    var ix = ex + (faceRoot.gazeX * 0.9);
                    var iy = eyeY + (faceRoot.gazeY * 0.7);
                    ctx.fillStyle = irisTone;
                    ctx.beginPath();
                    ctx.ellipse(ix - 0.108, iy - 0.108, 0.216, 0.216);
                    ctx.fill();
                    ctx.fillStyle = Qt.rgba(0.04, 0.02, 0.02, 1.0);
                    ctx.beginPath();
                    ctx.ellipse(ix - 0.050, iy - 0.050, 0.100, 0.100);
                    ctx.fill();
                    ctx.fillStyle = Qt.rgba(0.55 + (faceRoot.accent.r * 0.45), 0.55 + (faceRoot.accent.g * 0.45), 0.55 + (faceRoot.accent.b * 0.45), 0.80);
                    ctx.beginPath();
                    ctx.ellipse(ix - 0.092, iy - 0.096, 0.056, 0.056);
                    ctx.fill();
                    ctx.restore();
                }
                ctx.strokeStyle = lineInk;
                ctx.lineWidth = 0.042;
                ctx.beginPath();
                ctx.moveTo(ex - 0.225, eyeY + 0.010);
                ctx.quadraticCurveTo(ex, eyeY - (0.225 * lidOpen) - 0.012, ex + 0.225, eyeY + 0.010);
                ctx.stroke();
                ctx.strokeStyle = softInk;
                ctx.lineWidth = 0.028;
                ctx.beginPath();
                ctx.moveTo(ex - 0.19, eyeY + 0.028);
                ctx.quadraticCurveTo(ex, eyeY + (0.130 * lidOpen) + 0.028, ex + 0.20, eyeY + 0.018);
                ctx.stroke();
                var innerDrop = cynical ? 0.095 : 0.0;
                var outerLift = cynical ? 0.0 : (side > 0 ? 0.115 : 0.02);
                ctx.strokeStyle = lineInk;
                ctx.lineWidth = 0.075;
                ctx.beginPath();
                ctx.moveTo(ex - (side * 0.235), eyeY - 0.285 + innerDrop);
                ctx.quadraticCurveTo(ex, eyeY - 0.375 - (outerLift * 0.5), ex + (side * 0.245), eyeY - 0.300 - outerLift);
                ctx.stroke();
            }
            ctx.strokeStyle = softInk;
            ctx.lineWidth = 0.036;
            ctx.beginPath();
            ctx.moveTo(-0.055, eyeY + 0.08);
            ctx.quadraticCurveTo(-0.105, eyeY + 0.30, -0.020, eyeY + 0.355);
            ctx.stroke();
            ctx.fillStyle = Qt.rgba(0.14, 0.08, 0.05, 0.55);
            ctx.beginPath();
            ctx.ellipse(-0.075, eyeY + 0.325, 0.070, 0.045);
            ctx.fill();
            ctx.beginPath();
            ctx.ellipse(0.020, eyeY + 0.325, 0.070, 0.045);
            ctx.fill();
            var mouthY = 0.46;
            var halfW = 0.24 + (0.155 * faceRoot.mouthWide);
            var gap = 0.34 * faceRoot.mouthOpen;
            if (gap > 0.012) {
                ctx.fillStyle = mouthDark;
                ctx.beginPath();
                ctx.moveTo(-halfW, mouthY);
                ctx.quadraticCurveTo(0, mouthY - (gap * 0.60), halfW, mouthY);
                ctx.quadraticCurveTo(0, mouthY + gap, -halfW, mouthY);
                ctx.fill();
                ctx.strokeStyle = lipTone;
                ctx.lineWidth = 0.052;
                ctx.beginPath();
                ctx.moveTo(-halfW * 0.92, mouthY + (gap * 0.60));
                ctx.quadraticCurveTo(0, mouthY + gap + 0.062, halfW * 0.92, mouthY + (gap * 0.60));
                ctx.stroke();
            } else {
                var setDown = cynical ? 0.070 : 0.018;
                ctx.strokeStyle = lineInk;
                ctx.lineWidth = 0.070;
                ctx.beginPath();
                ctx.moveTo(-halfW, mouthY - setDown);
                ctx.quadraticCurveTo(0, mouthY + 0.055, halfW, mouthY + setDown);
                ctx.stroke();
                ctx.strokeStyle = Qt.rgba(lipTone.r, lipTone.g, lipTone.b, 0.45);
                ctx.lineWidth = 0.040;
                ctx.beginPath();
                ctx.moveTo(-halfW * 0.85, mouthY + 0.075 - setDown);
                ctx.quadraticCurveTo(0, mouthY + 0.125, halfW * 0.85, mouthY + 0.075 + setDown);
                ctx.stroke();
            }
            var jaw = ctx.createLinearGradient(0, mouthY + 0.20, 0, mouthY + 0.78);
            jaw.addColorStop(0.0, Qt.rgba(0.16, 0.09, 0.05, 0.0));
            jaw.addColorStop(1.0, Qt.rgba(0.16, 0.09, 0.05, 0.34));
            ctx.fillStyle = jaw;
            ctx.beginPath();
            ctx.moveTo(-0.62, mouthY + 0.20);
            ctx.quadraticCurveTo(0, mouthY + 0.86, 0.62, mouthY + 0.20);
            ctx.fill();
        }
    }
}
