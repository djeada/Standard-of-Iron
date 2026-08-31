import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron.Core 1.0 as Core
import "." as Design

ApplicationWindow {
    id: window

    width: 1100
    height: 800
    visible: true
    title: qsTr("Standard of Iron — Iron and Ember Gallery")
    color: Design.Theme.backgroundDeep

    Design.ToolShell {
        anchors.fill: parent

        toolbar: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.Metrics.space12
            anchors.rightMargin: Design.Metrics.space12
            spacing: Design.Metrics.space12

            Text {
                text: qsTr("Scale")
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
            }

            Design.IronSlider {
                Layout.preferredWidth: 160
                from: Core.UiPreferences.minUiScale
                to: Core.UiPreferences.maxUiScale
                stepSize: 0.05
                value: Core.UiPreferences.uiScale
                onMoved: Core.UiPreferences.uiScale = value
            }

            Design.IronCheckBox {
                text: qsTr("Reduce motion")
                checked: Core.UiPreferences.reducedMotion
                onToggled: Core.UiPreferences.reducedMotion = checked
            }

            Design.IronCheckBox {
                text: qsTr("High contrast")
                checked: Core.UiPreferences.highContrast
                onToggled: Core.UiPreferences.highContrast = checked
            }

            Design.IronDropdown {
                Layout.preferredWidth: 180
                model: Core.UiPreferences.colorVisionModes
                currentIndex: Math.max(0, Core.UiPreferences.colorVisionModes.indexOf(Core.UiPreferences.colorVisionMode))
                onActivated: function (index) {
                    Core.UiPreferences.colorVisionMode = Core.UiPreferences.colorVisionModes[index];
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Design.IronButton {
                text: qsTr("Reset")
                onClicked: Core.UiPreferences.reset_to_defaults()
            }
        }

        workspace: Design.ComponentGallery {
            anchors.fill: parent
        }

        statusBar: Text {
            anchors.fill: parent
            anchors.leftMargin: Design.Metrics.space12
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Iron and Ember — every control the product may use")
            color: Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
        }
    }
}
