import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Item {
    id: emptyHintRoot

    required property var panel
    readonly property var hs: StyleGuide.historical
    property real viewportHeight: 0
    property bool has_barracks: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("barracks")))
    property bool has_builder: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("builder")))
    property bool has_home: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("home")))
    property bool has_marketplace: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("marketplace")))
    property bool has_temple: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("temple")))
    property bool has_farm: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("farm")))

    property int owned_barracks: 0

    function refresh_owned_barracks() {
        var count = (typeof game !== "undefined" && game.orders && game.orders.owned_building_count) ? game.orders.owned_building_count("barracks") : 0;
        if (count >= 0)
            owned_barracks = count;
    }

    visible: !has_barracks && !has_builder && !has_home && !has_marketplace && !has_temple && !has_farm
    width: parent.width
    height: Math.max(emptyProductionHint.implicitHeight, viewportHeight)

    Column {
        id: emptyProductionHint

        anchors.centerIn: parent
        width: Math.max(0, parent.width - Design.Metrics.space16)
        spacing: 4

        Text {
            width: parent.width
            text: Design.Icons.unitGlyph("defense_tower")
            color: "#3B2F24"
            font.pixelSize: Design.Typography.glyphSmall
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            text: owned_barracks > 0 ? qsTr("Barracks") : qsTr("No Barracks")
            color: "#8D7146"
            font.pixelSize: Design.Typography.label
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: qsTr("Select a barracks to recruit units")
            color: Theme.textSubLite
            font.pixelSize: Design.Typography.caption
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Design.IronButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: owned_barracks > 0
            text: qsTr("Go to Barracks")
            tone: "primary"
            onClicked: {
                if (typeof game !== "undefined" && game.orders && game.orders.focus_owned_building)
                    game.orders.focus_owned_building("barracks");
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: emptyHintRoot.visible

        onTriggered: emptyHintRoot.refresh_owned_barracks()
    }
}
