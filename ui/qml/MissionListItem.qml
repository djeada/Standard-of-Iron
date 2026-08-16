import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio

Rectangle {
    id: root

    property var mission_data: null
    property bool is_selected: false
    readonly property string mission_glyph_prefix: "✦ "

    signal clicked

    readonly property string mission_title: {
        if (!mission_data)
            return "";
        if (mission_data.title)
            return mission_data.title;
        if (mission_data.mission_id && typeof game !== "undefined" && game.get_mission_definition) {
            var definition = game.get_mission_definition(mission_data.mission_id);
            if (definition && definition.title)
                return definition.title;
        }
        return titleize(mission_data.mission_id);
    }

    function titleize(value) {
        if (!value)
            return "";
        var parts = value.split("_");
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i];
            if (part.length === 0)
                continue;
            parts[i] = part.charAt(0).toUpperCase() + part.slice(1);
        }
        return parts.join(" ");
    }

    implicitHeight: Math.max(112, item_layout.implicitHeight + Theme.spacingMedium * 2)
    height: implicitHeight
    radius: Theme.radiusMedium
    gradient: Gradient {
        GradientStop {
            position: 0
            color: is_selected ? "#54372b" : (mouse_area.containsMouse ? "#4a3526" : "#372a1f")
        }

        GradientStop {
            position: 1
            color: is_selected ? "#39251e" : "#241c15"
        }
    }
    color: "transparent"
    border.color: is_selected ? "#c29555" : "#8f6d43"
    border.width: is_selected ? 2 : 1
    opacity: (mission_data && mission_data.unlocked === false) ? 0.5 : 1
    clip: true

    MouseArea {
        id: mouse_area

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onContainsMouseChanged: {
            if (containsMouse && typeof game !== "undefined")
                UiAudio.play_hover(game.audio_system);
        }
        onClicked: {
            if (typeof game !== "undefined")
                UiAudio.play_click(game.audio_system);
            root.clicked();
        }
    }

    RowLayout {
        id: item_layout

        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingMedium

        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            radius: 18
            color: mission_data && mission_data.completed ? "#355738" : "#7c3728"
            border.color: mission_data && mission_data.completed ? "#79a67a" : "#c29555"
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: mission_data ? Design.Numerals.ordinal(mission_data.order_index) : "?"
                color: "#f5e7c5"
                font.pixelSize: Design.Typography.bodyLarge
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingTiny

            RowLayout {
                id: heading_row

                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Label {
                    text: mission_glyph_prefix + root.mission_title
                    color: Theme.textMain
                    font.pixelSize: Design.Typography.bodyLarge
                    font.bold: true
                    font.family: "serif"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: mission_data && mission_data.completed
                    Layout.preferredWidth: archived_label.implicitWidth + 16
                    Layout.preferredHeight: 20
                    radius: Theme.radiusSmall
                    color: Theme.successBg
                    border.color: Theme.successBr
                    border.width: 1

                    Label {
                        id: archived_label

                        anchors.centerIn: parent
                        text: qsTr("✓ Archived")
                        color: Theme.successText
                        font.pixelSize: Design.Typography.label
                        font.bold: true
                    }
                }

                Rectangle {
                    visible: mission_data && mission_data.unlocked === false
                    Layout.preferredWidth: sealed_label.implicitWidth + 16
                    Layout.preferredHeight: 20
                    radius: Theme.radiusSmall
                    color: Theme.disabledBg
                    border.color: Theme.border
                    border.width: 1

                    Label {
                        id: sealed_label

                        anchors.centerIn: parent
                        text: Design.Icons.locked + " " + qsTr("Sealed")
                        color: Theme.textDim
                        font.pixelSize: Design.Typography.label
                    }
                }

                Rectangle {
                    visible: mission_data && mission_data.unlocked && !mission_data.completed
                    Layout.preferredWidth: open_order_label.implicitWidth + 16
                    Layout.preferredHeight: 20
                    radius: Theme.radiusSmall
                    color: Theme.infoBg
                    border.color: Theme.infoBr
                    border.width: 1

                    Label {
                        id: open_order_label

                        anchors.centerIn: parent
                        text: qsTr("Open Order")
                        color: Theme.infoText
                        font.pixelSize: Design.Typography.label
                        font.bold: true
                    }
                }
            }

            Label {
                id: mission_intro_label

                text: mission_data && mission_data.intro_text ? mission_data.intro_text : ""
                color: Theme.textSubLite
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                Layout.fillWidth: true
                font.pixelSize: Design.Typography.body
                font.family: "serif"
            }

            RowLayout {
                spacing: Theme.spacingTiny
                visible: !!(mission_data && mission_data.difficulty_modifier)

                Label {
                    text: qsTr("Difficulty:")
                    color: Theme.textDim
                    font.pixelSize: Design.Typography.label
                }

                Repeater {

                    model: 5

                    delegate: Text {
                        required property int index

                        readonly property int filled: {
                            if (!mission_data || !mission_data.difficulty_modifier)
                                return 1;
                            var steps = Math.ceil((mission_data.difficulty_modifier - 1.0) / 0.15);
                            return Math.max(1, Math.min(5, steps));
                        }

                        text: index < filled ? "★" : "☆"
                        color: index < filled ? Theme.warningText : Theme.textDim
                        font.pixelSize: Design.Typography.label
                    }
                }
            }
        }

        Text {
            id: advanceArrow

            text: "➤"
            font.pixelSize: Design.Typography.heading
            color: "#c29555"
            Layout.alignment: Qt.AlignVCenter

            transform: Scale {
                origin.x: advanceArrow.width / 2
                xScale: Design.Icons.mirrored ? -1 : 1
            }
        }
    }

    Behavior on color  {
        ColorAnimation {
            duration: Theme.animNormal
        }
    }

    Behavior on border.color  {
        ColorAnimation {
            duration: Theme.animNormal
        }
    }
}
