import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control

    property string buttonStyle: "primary"
    property var styleConfig: {
        if (buttonStyle === "primary")
            return StyleGuide.button.primary;

        if (buttonStyle === "secondary")
            return StyleGuide.button.secondary;

        if (buttonStyle === "small")
            return StyleGuide.button.small;

        if (buttonStyle === "danger")
            return StyleGuide.button.danger;

        return StyleGuide.button.primary;
    }

    implicitHeight: styleConfig.height
    implicitWidth: styleConfig.minWidth
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
        font.pointSize: control.enabled ? (hoverArea.containsMouse ? styleConfig.hoverFontSize : styleConfig.fontSize) : styleConfig.fontSize
        font.bold: true
        color: {
            if (!control.enabled)
                return styleConfig.disabledTextColor;

            if (buttonStyle === "danger" && hoverArea.containsMouse)
                return styleConfig.hoverTextColor || styleConfig.textColor;

            return styleConfig.textColor;
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
        implicitWidth: styleConfig.minWidth
        implicitHeight: styleConfig.height
        radius: styleConfig.radius
        color: {
            if (!control.enabled)
                return styleConfig.disabledBg;

            if (control.down)
                return styleConfig.pressBg;

            if (hoverArea.containsMouse)
                return styleConfig.hoverBg;

            return styleConfig.normalBg;
        }
        border.width: 1
        border.color: {
            if (!control.enabled)
                return styleConfig.disabledBorder;

            if (hoverArea.containsMouse)
                return styleConfig.hoverBorder;

            return styleConfig.normalBorder;
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

    }

}
