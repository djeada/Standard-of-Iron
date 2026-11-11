import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: plate

    property bool reachesRightEdge: false
    property var dividers: []

    readonly property int cornerRadius: plate.reachesRightEdge ? 0 : Design.A11y.scaled(12)
    readonly property int edgeWidth: Design.A11y.scaled(2)
    readonly property int shadowReach: Design.A11y.scaled(22)
    readonly property color iron: Qt.rgba(0.095, 0.078, 0.06, 0.94)
    readonly property color ironDeep: Qt.rgba(0.045, 0.037, 0.03, 0.97)
    readonly property color gilt: Qt.rgba(Design.Theme.accent.r, Design.Theme.accent.g, Design.Theme.accent.b, 0.82)
    readonly property color giltSoft: Qt.rgba(Design.Theme.accent.r, Design.Theme.accent.g, Design.Theme.accent.b, 0.42)

    Accessible.ignored: true

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: plate.cornerRadius
        anchors.bottom: body.top
        height: plate.shadowReach
        clip: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "#00000000"
                }
                GradientStop {
                    position: 1.0
                    color: "#5c000000"
                }
            }
        }
    }

    Item {
        id: body

        anchors.fill: parent
        clip: true

        Rectangle {
            x: -plate.cornerRadius - plate.edgeWidth
            y: 0
            width: parent.width + plate.cornerRadius + plate.edgeWidth
            height: parent.height + plate.cornerRadius + plate.edgeWidth
            radius: plate.cornerRadius
            border.width: plate.edgeWidth
            border.color: plate.gilt
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: plate.iron
                }
                GradientStop {
                    position: 1.0
                    color: plate.ironDeep
                }
            }
        }

        Rectangle {
            x: 0
            y: plate.edgeWidth
            width: parent.width - plate.cornerRadius
            height: 1
            color: "#8c000000"
        }

        Rectangle {
            x: 0
            y: plate.edgeWidth + 1
            width: parent.width - plate.cornerRadius
            height: Design.A11y.scaled(28)
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: Qt.rgba(1, 0.86, 0.6, 0.06)
                }
                GradientStop {
                    position: 1.0
                    color: "#00000000"
                }
            }
        }

        Repeater {
            model: plate.dividers

            delegate: Rectangle {
                required property var modelData

                x: Math.round(modelData) - 1
                y: Design.Metrics.space12
                width: 2
                height: body.height - Design.Metrics.space16 * 2
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: "#00000000"
                    }
                    GradientStop {
                        position: 0.5
                        color: plate.giltSoft
                    }
                    GradientStop {
                        position: 1.0
                        color: "#00000000"
                    }
                }
            }
        }
    }
}
