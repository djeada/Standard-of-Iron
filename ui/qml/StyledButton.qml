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

        Behavior on font.pointSize {
            NumberAnimation {
                duration: StyleGuide.animation.fast
            }

        }

        Behavior on color {
            ColorAnimation {
                duration: StyleGuide.animation.normal
            }

        }

    }

    background: Rectangle {
        implicitWidth: style_config.minWidth
        implicitHeight: style_config.height
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
        border.width: 1
        border.color: {
            if (!control.enabled)
                return style_config.disabledBorder;

            if (hoverArea.containsMouse)
                return style_config.hoverBorder;

            return style_config.normalBorder;
        }

        Behavior on color {
            ColorAnimation {
                duration: StyleGuide.animation.normal
            }

        }

        Behavior on border.color {
            ColorAnimation {
                duration: StyleGuide.animation.normal
            }

        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: Math.max(2, parent.radius - 2)
            color: "transparent"
            border.width: button_style === "primary" ? 1 : 0
            border.color: button_style === "primary" ? StyleGuide.historical.bronze : "transparent"
            opacity: 0.65
        }

    }

}
