import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Button {
    id: control

    property string button_style: "primary"
    property var style_config: {
        if (button_style === "primary")
            return StyleGuide.button.primary;
        if (button_style === "secondary")
            return StyleGuide.button.secondary;
        if (button_style === "small")
            return StyleGuide.button.small;
        if (button_style === "danger")
            return StyleGuide.button.danger;
        return StyleGuide.button.primary;
    }

    implicitHeight: style_config.height
    implicitWidth: style_config.minWidth
    hoverEnabled: true
    ToolTip.visible: control.ToolTip.text !== "" && hoverArea.containsMouse
    ToolTip.delay: 500

    MouseArea {
        id: hoverArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    contentItem: Text {
        text: control.text
        font.pointSize: control.enabled ? (hoverArea.containsMouse ? style_config.hoverFontSize : style_config.fontSize) : style_config.fontSize
        font.bold: true
        font.letterSpacing: style_config.letterSpacing !== undefined ? style_config.letterSpacing : 0.0
        color: {
            if (!control.enabled)
                return style_config.disabledTextColor;
            if (button_style === "danger" && hoverArea.containsMouse)
                return style_config.hoverTextColor || style_config.text_color;
            return style_config.text_color;
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        topPadding: control.down ? 1 : 0

        Behavior on font.pointSize  {
            NumberAnimation {
                duration: StyleGuide.animation.fast
            }
        }

        Behavior on color  {
            ColorAnimation {
                duration: StyleGuide.animation.normal
            }
        }

        Behavior on topPadding  {
            NumberAnimation {
                duration: StyleGuide.animation.fast
            }
        }
    }

    background: Item {
        implicitWidth: style_config.minWidth
        implicitHeight: style_config.height

        Rectangle {
            x: 1
            y: 2
            width: parent.width
            height: parent.height
            radius: style_config.radius + 1
            color: "#000000"
            opacity: control.enabled && !control.down ? 0.28 : 0.0

            Behavior on opacity  {
                NumberAnimation {
                    duration: StyleGuide.animation.fast
                }
            }
        }

        Rectangle {
            id: bodyRect

            anchors.fill: parent
            radius: style_config.radius
            color: {
                if (!control.enabled)
                    return style_config.disabledBg;
                if (control.down)
                    return style_config.pressBg;
                if (hoverArea.containsMouse)
                    return style_config.hoverBg;
                return style_config.normalBg;
            }
            border.width: button_style === "primary" ? 2 : 1
            border.color: {
                if (!control.enabled)
                    return style_config.disabledBorder;
                if (hoverArea.containsMouse)
                    return style_config.hoverBorder;
                return style_config.normalBorder;
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Math.max(1, parent.radius - 1)
                opacity: control.down ? 0.0 : 0.13
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: "#FFFFFF"
                    }

                    GradientStop {
                        position: 0.45
                        color: "#FFFFFF"
                    }

                    GradientStop {
                        position: 1.0
                        color: "#000000"
                    }
                }

                Behavior on opacity  {
                    NumberAnimation {
                        duration: StyleGuide.animation.fast
                    }
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.left: parent.left
                anchors.leftMargin: Math.max(4, parent.radius)
                anchors.right: parent.right
                anchors.rightMargin: Math.max(4, parent.radius)
                height: 1
                color: StyleGuide.palette.accentBright
                opacity: control.enabled && !control.down ? 0.55 : 0.0

                Behavior on opacity  {
                    NumberAnimation {
                        duration: StyleGuide.animation.fast
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Math.max(2, parent.radius - 2)
                color: "transparent"
                border.width: (button_style === "primary" || button_style === "secondary") ? 1 : 0
                border.color: StyleGuide.historical.bronze
                opacity: control.enabled && !control.down ? 0.42 : 0.0

                Behavior on opacity  {
                    NumberAnimation {
                        duration: StyleGuide.animation.fast
                    }
                }
            }

            Behavior on color  {
                ColorAnimation {
                    duration: StyleGuide.animation.normal
                }
            }

            Behavior on border.color  {
                ColorAnimation {
                    duration: StyleGuide.animation.normal
                }
            }
        }
    }
}
