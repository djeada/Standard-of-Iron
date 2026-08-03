import QtQuick 2.15
import StandardOfIron.Core 1.0 as Core
import ".." as Design

Canvas {
    id: root

    property string iconId: ""

    property color tint: Design.Theme.textPrimary
    property color accent: Design.Theme.accent
    property color ink: Design.Theme.backgroundDeep
    property color edge: Design.Theme.parchment
    property bool monochrome: false

    readonly property color timber: "#8a5a32"
    readonly property color quarry: "#8e8b86"
    readonly property color ore: "#7f8a96"
    readonly property color bullion: "#d9a441"

    readonly property bool available: iconId !== "" && Core.IconArt.has(iconId)
    readonly property var shapes: available ? Core.IconArt.strokes(iconId) : []

    implicitWidth: Design.Metrics.iconMedium
    implicitHeight: Design.Metrics.iconMedium
    antialiasing: true
    visible: available

    function toneColor(tone) {
        if (monochrome)
            return tone === "ink" ? Qt.rgba(tint.r, tint.g, tint.b, 0.45) : tint;
        switch (tone) {
        case "ink":
            return ink;
        case "edge":
            return edge;
        case "ember":
            return accent;
        case "timber":
            return timber;
        case "stone":
            return quarry;
        case "iron":
            return ore;
        case "gold":
            return bullion;
        }
        return tint;
    }

    onShapesChanged: requestPaint()
    onTintChanged: requestPaint()
    onAccentChanged: requestPaint()
    onInkChanged: requestPaint()
    onEdgeChanged: requestPaint()
    onMonochromeChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        var size = Math.min(width, height);
        if (size <= 0 || shapes.length === 0)
            return;
        var offsetX = (width - size) / 2;
        var offsetY = (height - size) / 2;
        for (var s = 0; s < shapes.length; ++s) {
            var shape = shapes[s];
            var color = toneColor(shape.tone);
            ctx.beginPath();
            for (var p = 0; p < shape.subpaths.length; ++p) {
                var points = shape.subpaths[p];
                for (var i = 0; i + 1 < points.length; i += 2) {
                    var px = offsetX + points[i] * size;
                    var py = offsetY + points[i + 1] * size;
                    if (i === 0)
                        ctx.moveTo(px, py);
                    else
                        ctx.lineTo(px, py);
                }
            }
            if (shape.filled) {
                ctx.fillStyle = color;
                ctx.fill();
            } else {
                ctx.strokeStyle = color;
                ctx.lineWidth = Math.max(1, shape.width * size);
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.stroke();
            }
        }
    }
}
