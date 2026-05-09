import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var mission_data: null
    property bool is_selected: false
    readonly property string mission_glyph_prefix: "✦ "

    signal clicked()

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

    height: 90
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
        onClicked: root.clicked()
    }

    RowLayout {
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
                text: mission_data ? (mission_data.order_index + 1).toString() : "?"
                color: "#f5e7c5"
                font.pointSize: Theme.fontSizeMedium
                font.bold: true
            }

        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingTiny

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Label {
                    text: mission_glyph_prefix + (mission_data && mission_data.mission_id ? titleize(mission_data.mission_id) : "")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeLarge
                    font.bold: true
                    font.family: "serif"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: mission_data && mission_data.completed
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 20
                    radius: Theme.radiusSmall
                    color: Theme.successBg
                    border.color: Theme.successBr
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: qsTr("✓ Archived")
                        color: Theme.successText
                        font.pointSize: Theme.fontSizeTiny
                        font.bold: true
                    }

                }

                Rectangle {
                    visible: mission_data && mission_data.unlocked === false
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 20
                    radius: Theme.radiusSmall
                    color: Theme.disabledBg
                    border.color: Theme.border
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: qsTr("🔒 Sealed")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeTiny
                    }

                }

                Rectangle {
                    visible: mission_data && mission_data.unlocked && !mission_data.completed
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 20
                    radius: Theme.radiusSmall
                    color: Theme.infoBg
                    border.color: Theme.infoBr
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: qsTr("Open Order")
                        color: Theme.infoText
                        font.pointSize: Theme.fontSizeTiny
                        font.bold: true
                    }

                }

            }

            Label {
                text: mission_data && mission_data.intro_text ? mission_data.intro_text : ""
                color: Theme.textSubLite
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                Layout.fillWidth: true
                font.pointSize: Theme.fontSizeSmall
                font.family: "serif"
            }

            RowLayout {
                spacing: Theme.spacingTiny
                visible: !!(mission_data && mission_data.difficulty_modifier)

                Label {
                    text: qsTr("Difficulty:")
                    color: Theme.textDim
                    font.pointSize: Theme.fontSizeTiny
                }

                Repeater {
                    model: {
                        if (!mission_data || !mission_data.difficulty_modifier)
                            return 1;

                        return Math.round(mission_data.difficulty_modifier);
                    }

                    delegate: Text {
                        text: "★"
                        color: Theme.warningText
                        font.pointSize: Theme.fontSizeTiny
                    }

                }

            }

        }

        Text {
            text: "➤"
            font.pointSize: Theme.fontSizeTitle
            color: "#c29555"
            Layout.alignment: Qt.AlignVCenter
        }

    }

    Behavior on color {
        ColorAnimation {
            duration: Theme.animNormal
        }

    }

    Behavior on border.color {
        ColorAnimation {
            duration: Theme.animNormal
        }

    }

}
