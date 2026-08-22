import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var intentData: (typeof game !== 'undefined' && game.orders) ? game.orders.context_intent : null
    property string intent: root.intentData ? (root.intentData.intent || "invalid") : "invalid"
    property bool intentValid: root.intentData ? !!root.intentData.valid : false
    property string reason: root.intentData ? (root.intentData.reason || "") : ""
    property real cursorX: 0
    property real cursorY: 0
    property bool tracking: false
    property string lastFailure: ""
    property string lastFailureText: ""

    readonly property color moveColor: "#7FD4A0"
    readonly property color attackColor: "#E4685D"
    readonly property color interactColor: "#F0C46A"
    readonly property color rallyColor: "#8FB8E8"
    readonly property color invalidColor: "#9AA0A6"

    readonly property color activeColor: {
        switch (root.intent) {
        case "move":
            return root.moveColor;
        case "attack":
            return root.attackColor;
        case "interact":
            return root.interactColor;
        case "rally":
            return root.rallyColor;
        default:
            return root.invalidColor;
        }
    }

    function acknowledge() {
        acknowledgeFlash.restart();
    }

    function report_outcome(accepted, message, failure) {
        root.lastFailure = accepted ? "" : (failure || "command_unavailable");
        root.lastFailureText = accepted ? "" : (message || "");
        if (accepted) {
            acknowledgeFlash.restart();
        } else {
            refusalHold.restart();
            refusalFlash.restart();
        }
    }

    visible: root.tracking && root.intent !== ""
    z: 999997

    Item {
        id: marker

        x: root.cursorX - width * 0.5
        y: root.cursorY - height * 0.5
        width: 40
        height: 40

        Canvas {
            id: markerCanvas

            anchors.fill: parent
            opacity: root.intentValid ? 0.95 : 0.75

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.lineWidth = 2;
                ctx.strokeStyle = root.activeColor;
                ctx.fillStyle = root.activeColor;
                var cx = width * 0.5;
                var cy = height * 0.5;
                if (root.intent === "move") {
                    ctx.beginPath();
                    ctx.ellipse(cx - 13, cy - 7, 26, 14);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.ellipse(cx - 4, cy - 2, 8, 4);
                    ctx.fill();
                } else if (root.intent === "attack") {
                    ctx.beginPath();
                    ctx.arc(cx, cy, 12, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(cx, cy - 17);
                    ctx.lineTo(cx, cy - 6);
                    ctx.moveTo(cx, cy + 6);
                    ctx.lineTo(cx, cy + 17);
                    ctx.moveTo(cx - 17, cy);
                    ctx.lineTo(cx - 6, cy);
                    ctx.moveTo(cx + 6, cy);
                    ctx.lineTo(cx + 17, cy);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
                    ctx.fill();
                } else if (root.intent === "interact") {
                    ctx.beginPath();
                    ctx.moveTo(cx, cy - 14);
                    ctx.lineTo(cx + 14, cy);
                    ctx.lineTo(cx, cy + 14);
                    ctx.lineTo(cx - 14, cy);
                    ctx.closePath();
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(cx, cy, 3.5, 0, Math.PI * 2);
                    ctx.fill();
                } else if (root.intent === "rally") {
                    ctx.beginPath();
                    ctx.moveTo(cx - 7, cy + 15);
                    ctx.lineTo(cx - 7, cy - 15);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(cx - 7, cy - 15);
                    ctx.lineTo(cx + 12, cy - 10);
                    ctx.lineTo(cx - 7, cy - 4);
                    ctx.closePath();
                    ctx.fill();
                } else {
                    ctx.beginPath();
                    ctx.arc(cx, cy, 12, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(cx - 8, cy - 8);
                    ctx.lineTo(cx + 8, cy + 8);
                    ctx.stroke();
                }
            }
        }

        Connections {
            function onIntentChanged() {
                markerCanvas.requestPaint();
            }

            function onIntentValidChanged() {
                markerCanvas.requestPaint();
            }

            target: root
        }

        SequentialAnimation {
            id: acknowledgeFlash

            NumberAnimation {
                target: marker
                property: "scale"
                from: 1.0
                to: 1.35
                duration: 90
            }

            NumberAnimation {
                target: marker
                property: "scale"
                from: 1.35
                to: 1.0
                duration: 180
            }
        }
    }

    Timer {
        id: refusalHold

        interval: 1600
        repeat: false
        onTriggered: {
            root.lastFailure = "";
            root.lastFailureText = "";
        }
    }

    SequentialAnimation {
        id: refusalFlash

        NumberAnimation {
            target: refusalRing
            property: "opacity"
            from: 0.0
            to: 0.9
            duration: 60
        }

        NumberAnimation {
            target: refusalRing
            property: "opacity"
            from: 0.9
            to: 0.0
            duration: 420
        }
    }

    Rectangle {
        id: refusalRing

        x: root.cursorX - width * 0.5
        y: root.cursorY - height * 0.5
        width: 46
        height: 46
        radius: width * 0.5
        color: "transparent"
        border.color: root.attackColor
        border.width: 3
        opacity: 0.0
    }

    Rectangle {
        id: reasonChip

        readonly property string chipText: root.lastFailureText.length > 0 ? root.lastFailureText : root.reason

        x: root.cursorX + 22
        y: root.cursorY + 14
        width: reasonLabel.implicitWidth + 12
        height: reasonLabel.implicitHeight + 8
        radius: 4
        color: "#CC1A1A1A"
        border.color: root.lastFailure.length > 0 ? root.attackColor : root.invalidColor
        border.width: 1
        visible: reasonChip.chipText.length > 0 && (root.lastFailure.length > 0 || !root.intentValid)

        Text {
            id: reasonLabel

            anchors.centerIn: parent
            color: "#E8E8E8"
            font.pixelSize: Design.Typography.caption
            text: reasonChip.chipText
        }
    }
}
