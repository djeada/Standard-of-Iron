import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    required property var panel
    readonly property var hs: StyleGuide.historical
    property bool has_barracks: (panel.selection_tick, (panel.production && panel.production.has_selected_type && (panel.production.has_selected_type("barracks") || panel.production.has_selected_type("temple"))))

    width: parent.width
    height: rallyContent.height + 12
    color: "#120D09"
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_barracks

    Column {
        id: rallyContent

        property var prod: (panel.selection_tick, (panel.production && panel.production.selected_state) ? panel.production.selected_state() : panel.default_production_state())
        property var temple_prod: (panel.selection_tick, (panel.production && panel.production.selected_temple_state) ? panel.production.selected_temple_state() : panel.default_production_state())
        property bool placing_barracks_rally: typeof gameView !== 'undefined' && gameView.cursor_mode === "place_barracks_rally"

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 6

        Button {
            id: rallyButton

            readonly property bool allowed: rallyContent.prod.has_barracks === true || rallyContent.temple_prod.has_temple === true

            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.parent.width - 20
            height: Design.A11y.scaled(32)
            text: rallyContent.placing_barracks_rally ? Design.Icons.rally + " " + qsTr("Click Map to Set Rally") : Design.Icons.rally + " " + qsTr("Set Rally Point")
            focusPolicy: Qt.NoFocus
            onClicked: {
                if (!allowed) {
                    Design.UiSound.warning();
                    return;
                }
                Design.UiSound.activate();
                panel.rally_mode_toggled();
            }
            ToolTip.visible: hovered
            ToolTip.text: allowed ? qsTr("Set where newly recruited units will gather.\nRight-click to cancel.") : qsTr("Select a barracks or temple before setting a rally point.")
            ToolTip.delay: 500

            background: Rectangle {
                color: rallyButton.allowed ? (rallyButton.down ? hs.bronzeDeep : (rallyButton.hovered ? hs.bronze : hs.parchmentDark)) : Theme.bgShade
                radius: 6
                border.color: rallyContent.placing_barracks_rally ? hs.bronze : hs.bronzeDeep
                border.width: 2
            }

            contentItem: Text {
                text: rallyButton.text
                font.pixelSize: Design.Typography.caption
                font.bold: true
                color: rallyButton.allowed ? "#F4E7C8" : "#6B5231"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: rallyContent.placing_barracks_rally ? qsTr("Right-click to cancel") : ""
            color: "#8D7146"
            font.pixelSize: Design.Typography.caption
            font.italic: true
        }
    }
}
