import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property string mode: "normal"
    property bool rallyPlacement: false
    property bool placingConstruction: false
    property bool constructionPreviewActive: false
    property bool constructionPreviewValid: false
    property string constructionPreviewReason: ""
    property real pointerX: 0
    property real pointerY: 0
    property var intentData: null
    property var attackHint: null
    property var interactionHint: null

    readonly property string intent: {
        if (!root.intentData)
            return "none";
        if (root.intentData.advises === false)
            return "none";
        return root.intentData.intent || "none";
    }
    readonly property bool intentValid: root.intentData ? !!root.intentData.valid : false
    readonly property string intentReason: root.intentData ? (root.intentData.reason || "") : ""

    readonly property bool armed: root.mode !== "normal"
    readonly property bool advisesOnPointer: !root.armed && (root.intent === "attack" || root.intent === "interact" || root.intent === "invalid")

    readonly property bool replacesPointer: root.armed || root.advisesOnPointer

    readonly property color moveColor: "#7FD4A0"
    readonly property color attackColor: "#E4685D"
    readonly property color interactColor: "#F0C46A"
    readonly property color rallyColor: "#8FB8E8"
    readonly property color invalidColor: "#9AA0A6"

    readonly property color intentColor: {
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

    function report_order_feedback(kind, accepted, message) {
        if (!message || message.length === 0)
            return;
        chip.orderKind = kind;
        chip.orderAccepted = accepted;
        chip.orderMessage = message;
        chipDismiss.interval = accepted ? 1400 : 2600;
        chipDismiss.restart();
    }

    function acknowledge() {
        acknowledgeFlash.restart();
    }

    function clear_order_feedback() {
        chipDismiss.stop();
        chip.orderMessage = "";
    }

    Item {
        id: glyphs

        width: 32
        height: 32
        z: 999999
        visible: root.replacesPointer
        x: root.pointerX - 16
        y: root.pointerY - 16

        property real acknowledgeScale: 1

        scale: glyphs.acknowledgeScale

        SequentialAnimation {
            id: acknowledgeFlash

            NumberAnimation {
                target: glyphs
                property: "acknowledgeScale"
                from: 1
                to: 1.35
                duration: 90
            }

            NumberAnimation {
                target: glyphs
                property: "acknowledgeScale"
                from: 1.35
                to: 1
                duration: 180
            }
        }

        Canvas {
            id: intentCursor

            anchors.fill: parent
            visible: root.advisesOnPointer
            onVisibleChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.lineWidth = 2.5;
                ctx.strokeStyle = root.intentColor;
                ctx.fillStyle = root.intentColor;
                var cx = 16;
                var cy = 16;
                if (root.intent === "attack") {
                    ctx.beginPath();
                    ctx.arc(cx, cy, 9, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(cx, cy - 14);
                    ctx.lineTo(cx, cy - 5);
                    ctx.moveTo(cx, cy + 5);
                    ctx.lineTo(cx, cy + 14);
                    ctx.moveTo(cx - 14, cy);
                    ctx.lineTo(cx - 5, cy);
                    ctx.moveTo(cx + 5, cy);
                    ctx.lineTo(cx + 14, cy);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(cx, cy, 2.5, 0, Math.PI * 2);
                    ctx.fill();
                } else if (root.intent === "interact") {
                    ctx.beginPath();
                    ctx.moveTo(cx, cy - 11);
                    ctx.lineTo(cx + 11, cy);
                    ctx.lineTo(cx, cy + 11);
                    ctx.lineTo(cx - 11, cy);
                    ctx.closePath();
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
                    ctx.fill();
                } else {
                    ctx.beginPath();
                    ctx.arc(cx, cy, 9, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(cx - 6, cy - 6);
                    ctx.lineTo(cx + 6, cy + 6);
                    ctx.stroke();
                }
            }
            Component.onCompleted: requestPaint()

            Connections {
                function onIntentChanged() {
                    intentCursor.requestPaint();
                }

                function onIntentColorChanged() {
                    intentCursor.requestPaint();
                }

                target: root
            }
        }

        Item {
            id: attackCursorContainer

            property real pulse_scale: 1

            visible: root.mode === "attack"
            anchors.fill: parent

            Canvas {
                id: attackCursor

                anchors.fill: parent
                scale: attackCursorContainer.pulse_scale
                transformOrigin: Item.Center
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = Theme.accentBright;
                    ctx.lineWidth = 3;
                    ctx.beginPath();
                    ctx.moveTo(16, 4);
                    ctx.lineTo(16, 28);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(4, 16);
                    ctx.lineTo(28, 16);
                    ctx.stroke();
                    ctx.fillStyle = Theme.accentBright;
                    ctx.beginPath();
                    ctx.arc(16, 16, 4, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.strokeStyle = Qt.rgba(0.75, 0.27, 0.18, 0.5);
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.arc(16, 16, 7, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.strokeStyle = Theme.accentBright;
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(8, 12);
                    ctx.lineTo(8, 8);
                    ctx.lineTo(12, 8);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(20, 8);
                    ctx.lineTo(24, 8);
                    ctx.lineTo(24, 12);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(8, 20);
                    ctx.lineTo(8, 24);
                    ctx.lineTo(12, 24);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(20, 24);
                    ctx.lineTo(24, 24);
                    ctx.lineTo(24, 20);
                    ctx.stroke();
                }
                Component.onCompleted: requestPaint()
            }

            SequentialAnimation on pulse_scale  {
                running: attackCursorContainer.visible
                loops: Animation.Infinite

                NumberAnimation {
                    from: 1
                    to: 1.2
                    duration: 400
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    from: 1.2
                    to: 1
                    duration: 400
                    easing.type: Easing.InOutQuad
                }
            }
        }

        Canvas {
            id: guardCursor

            visible: root.mode === "guard"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.fillStyle = Theme.thumbBr;
                ctx.strokeStyle = Theme.panelBr;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(16, 6);
                ctx.lineTo(24, 10);
                ctx.lineTo(24, 18);
                ctx.lineTo(16, 26);
                ctx.lineTo(8, 18);
                ctx.lineTo(8, 10);
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
                ctx.strokeStyle = Theme.textMain;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(13, 16);
                ctx.lineTo(15, 18);
                ctx.lineTo(19, 12);
                ctx.stroke();
            }
            Component.onCompleted: requestPaint()
        }

        Canvas {
            id: patrolCursor

            visible: root.mode === "patrol"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = Theme.accent;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.arc(16, 16, 10, 0, Math.PI * 2);
                ctx.stroke();
                ctx.fillStyle = Theme.accent;
                ctx.beginPath();
                ctx.moveTo(26, 16);
                ctx.lineTo(22, 13);
                ctx.lineTo(22, 19);
                ctx.closePath();
                ctx.fill();
                ctx.beginPath();
                ctx.moveTo(6, 16);
                ctx.lineTo(10, 13);
                ctx.lineTo(10, 19);
                ctx.closePath();
                ctx.fill();
                ctx.beginPath();
                ctx.arc(16, 16, 3, 0, Math.PI * 2);
                ctx.fill();
            }
            Component.onCompleted: requestPaint()
        }

        Canvas {
            id: deliverCursor

            visible: root.mode === "deliver"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = Theme.accentBright;
                ctx.fillStyle = StyleGuide.historical.wax;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.arc(16, 16, 11, 0, Math.PI * 2);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(9, 16);
                ctx.lineTo(23, 16);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(16, 9);
                ctx.lineTo(16, 23);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(24, 16);
                ctx.lineTo(18, 11);
                ctx.lineTo(18, 21);
                ctx.closePath();
                ctx.fill();
            }
            Component.onCompleted: requestPaint()
        }

        Item {
            id: rallyPlacementCursor

            visible: root.rallyPlacement
            anchors.fill: parent

            Image {
                anchors.centerIn: parent
                width: 28
                height: 28
                source: StyleGuide.icon_path("rally_mode.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }
        }

        Canvas {
            id: constructionCursor

            visible: root.placingConstruction && root.mode !== "collect"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                var active = root.constructionPreviewActive;
                var ok = active && root.constructionPreviewValid;
                var primary = !active ? "#D8C17A" : (ok ? "#75D36B" : "#D36060");
                var secondary = !active ? "#51401A" : (ok ? "#163A16" : "#4A1717");
                ctx.strokeStyle = primary;
                ctx.lineWidth = 2.5;
                ctx.beginPath();
                ctx.arc(16, 16, 10, 0, Math.PI * 2);
                ctx.stroke();
                ctx.fillStyle = Qt.rgba(0, 0, 0, 0.25);
                ctx.beginPath();
                ctx.arc(16, 16, 6, 0, Math.PI * 2);
                ctx.fill();
                ctx.strokeStyle = secondary;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(16, 5);
                ctx.lineTo(16, 11);
                ctx.moveTo(16, 21);
                ctx.lineTo(16, 27);
                ctx.moveTo(5, 16);
                ctx.lineTo(11, 16);
                ctx.moveTo(21, 16);
                ctx.lineTo(27, 16);
                ctx.stroke();
                ctx.strokeStyle = primary;
                ctx.lineWidth = 2.5;
                if (!active) {
                    ctx.beginPath();
                    ctx.moveTo(16, 10);
                    ctx.lineTo(16, 22);
                    ctx.moveTo(10, 16);
                    ctx.lineTo(22, 16);
                    ctx.stroke();
                } else if (ok) {
                    ctx.beginPath();
                    ctx.moveTo(11, 17);
                    ctx.lineTo(15, 21);
                    ctx.lineTo(22, 12);
                    ctx.stroke();
                } else {
                    ctx.beginPath();
                    ctx.moveTo(11, 11);
                    ctx.lineTo(21, 21);
                    ctx.moveTo(21, 11);
                    ctx.lineTo(11, 21);
                    ctx.stroke();
                }
            }
            Component.onCompleted: requestPaint()
        }

        Design.IronVectorIcon {
            id: repairCursor

            visible: root.mode === "repair"
            anchors.centerIn: parent
            width: 22
            height: 22
            iconId: "repair"
        }

        Item {
            id: collectCursorContainer

            visible: root.mode === "collect"
            anchors.fill: parent

            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                source: StyleGuide.icon_path("collect_mode.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                opacity: !root.constructionPreviewActive ? 0.9 : (root.constructionPreviewValid ? 1 : 0.7)
            }

            Canvas {
                id: collectCursorOverlay

                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    var active = root.constructionPreviewActive;
                    var ok = active && root.constructionPreviewValid;
                    var primary = !active ? "#D8C17A" : (ok ? "#75D36B" : "#D36060");
                    var secondary = !active ? "#51401A" : (ok ? "#163A16" : "#4A1717");
                    ctx.strokeStyle = primary;
                    ctx.lineWidth = 2.5;
                    ctx.beginPath();
                    ctx.arc(16, 16, 10, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.strokeStyle = secondary;
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(16, 5);
                    ctx.lineTo(16, 9);
                    ctx.moveTo(16, 23);
                    ctx.lineTo(16, 27);
                    ctx.moveTo(5, 16);
                    ctx.lineTo(9, 16);
                    ctx.moveTo(23, 16);
                    ctx.lineTo(27, 16);
                    ctx.stroke();
                    ctx.strokeStyle = primary;
                    ctx.lineWidth = 2.5;
                    ctx.fillStyle = primary;
                    ctx.beginPath();
                    ctx.arc(11, 11, 2, 0, Math.PI * 2);
                    ctx.arc(21, 11, 2, 0, Math.PI * 2);
                    ctx.arc(16, 22, 2, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.strokeStyle = primary;
                    ctx.lineWidth = 2.5;
                    if (!active) {
                        ctx.beginPath();
                        ctx.moveTo(16, 10);
                        ctx.lineTo(16, 22);
                        ctx.moveTo(10, 16);
                        ctx.lineTo(22, 16);
                        ctx.stroke();
                    } else if (ok) {
                        ctx.beginPath();
                        ctx.moveTo(11, 17);
                        ctx.lineTo(15, 21);
                        ctx.lineTo(22, 12);
                        ctx.stroke();
                    } else {
                        ctx.beginPath();
                        ctx.moveTo(11, 11);
                        ctx.lineTo(21, 21);
                        ctx.moveTo(21, 11);
                        ctx.lineTo(11, 21);
                        ctx.stroke();
                    }
                }
                Component.onCompleted: requestPaint()
            }
        }
    }

    function refresh_construction_glyphs() {
        constructionCursor.requestPaint();
        collectCursorOverlay.requestPaint();
    }

    Rectangle {
        id: chip

        property string orderKind: ""
        property bool orderAccepted: true
        property string orderMessage: ""

        readonly property var attack: root.attackHint ? root.attackHint : ({
                "state": "none",
                "name": "",
                "range": "none"
            })
        readonly property var interaction: root.interactionHint ? root.interactionHint : ({
                "action": "none"
            })

        function order_glyph() {
            if (!chip.orderAccepted)
                return "⊘";
            switch (chip.orderKind) {
            case "attack":
                return "⚔";
            case "move":
                return "➜";
            case "guard":
                return "⛨";
            case "patrol":
                return "↻";
            case "hold":
                return "⏸";
            case "stop":
                return "■";
            case "build":
                return "⚒";
            case "gather":
                return "⛏";
            case "deliver":
                return "⌂";
            case "repair":
                return "⚒";
            case "rally":
                return "⚑";
            case "formation":
                return "☷";
            }
            return "✓";
        }

        function attack_text() {
            switch (chip.attack.state) {
            case "valid":
                return chip.attack.name && chip.attack.name.length > 0 ? chip.attack.name : qsTr("Enemy target");
            case "ally":
                return qsTr("Cannot attack ally");
            case "neutral":
                return qsTr("Cannot attack this target");
            case "no_attackers":
                return qsTr("Selection cannot attack");
            }
            return "";
        }

        function attack_range_text() {
            switch (chip.attack.range) {
            case "in_range":
                return qsTr("In range");
            case "too_close":
                return qsTr("Too close");
            case "out_of_range":
                return qsTr("Out of range");
            case "blocked":
                return qsTr("No firing line");
            }
            return "";
        }

        function attack_range_glyph() {
            switch (chip.attack.range) {
            case "in_range":
                return "◎";
            case "too_close":
                return "△";
            case "out_of_range":
                return "○";
            case "blocked":
                return "✕";
            }
            return "";
        }

        function interaction_text() {
            switch (chip.interaction.action) {
            case "gather":
                return qsTr("Collect");
            case "deliver":
                return qsTr("Deliver civilians");
            case "repair":
                return qsTr("Repair");
            case "harvest":
                return qsTr("Harvest grain");
            case "slaughter":
                return qsTr("Slaughter sheep");
            }
            return "";
        }

        function interaction_glyph() {
            switch (chip.interaction.action) {
            case "gather":
                return Design.Icons.collect;
            case "deliver":
                return Design.Icons.deliver;
            case "repair":
                return "⚒";
            case "harvest":
                return Design.Icons.collect;
            case "slaughter":
                return Design.Icons.collect;
            }
            return "";
        }

        readonly property string source: {
            if (chip.orderMessage.length > 0)
                return "order";
            if (root.placingConstruction && root.constructionPreviewActive && !root.constructionPreviewValid && root.constructionPreviewReason.length > 0)
                return "placement";
            if (root.intent === "invalid" && root.intentReason.length > 0)
                return "refusal";
            if (root.mode === "attack" && chip.attack.state !== "none")
                return "attack";
            if (root.mode === "normal" && chip.interaction.action !== "none")
                return "interaction";
            return "";
        }

        readonly property string glyph: {
            switch (chip.source) {
            case "order":
                return chip.order_glyph();
            case "placement":
            case "refusal":
                return "⊘";
            case "attack":
                return chip.attack.state === "valid" ? "⚔" : "⊘";
            case "interaction":
                return chip.interaction_glyph();
            }
            return "";
        }

        readonly property string label: {
            switch (chip.source) {
            case "order":
                return chip.orderMessage;
            case "placement":
                return root.constructionPreviewReason;
            case "refusal":
                return root.intentReason;
            case "attack":
                return chip.attack_text();
            case "interaction":
                return chip.interaction_text();
            }
            return "";
        }

        readonly property string detail: chip.source === "attack" ? chip.attack_range_text() : ""

        readonly property bool negative: chip.source === "placement" || chip.source === "refusal" || (chip.source === "order" && !chip.orderAccepted) || (chip.source === "attack" && chip.attack.state !== "valid")

        visible: chip.source !== "" && chip.label.length > 0
        z: 999998
        radius: 5
        color: "#C8141414"
        border.color: chip.negative ? Theme.dangerBr : Theme.panelBr
        border.width: 1
        width: chipRow.implicitWidth + 16
        height: chipRow.implicitHeight + 8
        x: Math.min(Math.max(0, root.pointerX + 22), root.width - width)
        y: Math.min(Math.max(0, root.pointerY + 24), root.height - height)

        Timer {
            id: chipDismiss

            interval: 1400
            repeat: false
            onTriggered: chip.orderMessage = ""
        }

        Row {
            id: chipRow

            anchors.centerIn: parent
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: chip.glyph.length > 0
                color: chip.negative ? Theme.dangerBr : Theme.accent
                text: chip.glyph
                font.pixelSize: Design.Typography.label
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: chip.negative ? Theme.warningText : Theme.textMain
                text: chip.label
                font.pixelSize: Design.Typography.caption
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: chip.detail.length > 0
                color: chip.attack.range === "in_range" ? Theme.successText : Theme.warningText
                text: chip.attack_range_glyph() + " " + chip.detail
                font.pixelSize: Design.Typography.caption
            }
        }
    }
}
