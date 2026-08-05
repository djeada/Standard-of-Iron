import QtQuick 2.15

Item {
    id: shape

    property string pattern: ""
    property color tone: "#ffffff"
    property color accentTone: "#ffffff"
    property real dotSize: 3
    property real gap: 2

    readonly property int columns: 7
    readonly property int rows: 4

    implicitWidth: columns * dotSize + (columns - 1) * gap
    implicitHeight: rows * dotSize + (rows - 1) * gap

    Repeater {
        model: shape.columns * shape.rows

        Rectangle {
            readonly property string cell: shape.pattern.length > index ? shape.pattern.charAt(index) : "."

            color: cell === "@" ? shape.accentTone : shape.tone
            height: shape.dotSize
            radius: shape.dotSize / 2
            visible: cell !== "."
            width: shape.dotSize
            x: (index % shape.columns) * (shape.dotSize + shape.gap)
            y: Math.floor(index / shape.columns) * (shape.dotSize + shape.gap)
        }
    }
}
