import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    id: row

    property bool selected: false
    property bool loadable: true
    property string blocked_reason: ""
    property string slot_name: ""
    property string title: ""
    property string map_name: ""
    property string mode: ""
    property string mission_label: ""
    property string kind: "manual"
    property string timestamp: ""
    property real play_time_seconds: 0
    property string thumbnail: ""
    property Component actions: null

    signal clicked
    signal double_clicked

    function format_play_time(seconds) {
        if (!seconds || seconds <= 0)
            return "";
        var total = Math.floor(seconds);
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        if (hours > 0)
            return qsTr("%1h %2m").arg(hours).arg(minutes);
        if (minutes > 0)
            return qsTr("%1m").arg(minutes);
        return qsTr("under a minute");
    }

    function format_timestamp(value) {
        if (!value)
            return qsTr("Unknown");
        var when = new Date(value);
        if (isNaN(when.getTime()))
            return qsTr("Unknown");
        return Qt.formatDateTime(when, "d MMM yyyy · hh:mm");
    }

    function describe_mode() {
        if (row.mode === "campaign")
            return qsTr("Campaign");
        if (row.mode === "mission")
            return qsTr("Mission");
        return qsTr("Skirmish");
    }

    function kind_label() {
        if (row.kind === "autosave")
            return qsTr("Autosave");
        if (row.kind === "quicksave")
            return qsTr("Quicksave");
        return "";
    }

    height: Math.max(112, layout.implicitHeight + Theme.spacingMedium * 2)
    radius: Theme.radiusMedium
    color: row.selected ? Qt.rgba(0.86, 0.72, 0.40, 0.15) : hover_area.containsMouse ? Theme.hoverBg : Qt.rgba(0, 0, 0, 0)
    border.color: row.selected ? Theme.accent : hover_area.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Theme.cardBorder
    border.width: row.selected ? 2 : 1
    opacity: row.loadable ? 1 : 0.72

    MouseArea {
        id: hover_area

        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            Design.UiSound.activate();
            row.clicked();
        }
        onDoubleClicked: row.double_clicked()
        onContainsMouseChanged: {
            if (containsMouse)
                Design.UiSound.hover();
        }
    }

    RowLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingMedium

        Rectangle {
            Layout.preferredWidth: 128
            Layout.preferredHeight: 80
            radius: Theme.radiusSmall
            color: Theme.cardBase
            border.color: Theme.cardBorder
            border.width: 1
            clip: true

            Image {
                id: preview

                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                source: row.thumbnail && row.thumbnail.length > 0 ? "data:image/png;base64," + row.thumbnail : ""
                visible: source !== "" && status === Image.Ready

                opacity: row.loadable ? 1 : 0.5
            }

            Label {
                anchors.centerIn: parent
                visible: !preview.visible
                text: qsTr("No preview")
                color: Theme.textHint
                font.pixelSize: Design.Typography.label
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingTiny

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Label {
                    id: title_label

                    text: row.title !== "" ? row.title : row.slot_name
                    color: Theme.textMain
                    font.pixelSize: Design.Typography.subheading
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Label.ElideRight
                }

                Rectangle {
                    visible: row.kind_label() !== ""
                    radius: Theme.radiusSmall
                    color: Theme.cardBaseB
                    border.color: Theme.thumbBr
                    border.width: 1
                    implicitWidth: kind_text.implicitWidth + Theme.spacingSmall * 2

                    implicitHeight: title_label.implicitHeight

                    Label {
                        id: kind_text

                        anchors.centerIn: parent
                        text: row.kind_label()
                        color: Theme.textSubLite
                        font.pixelSize: Design.Typography.caption
                    }
                }
            }

            Label {
                text: row.mission_label !== "" ? qsTr("%1 · %2 · %3").arg(row.mission_label).arg(row.describe_mode()).arg(row.map_name) : qsTr("%1 · %2").arg(row.map_name).arg(row.describe_mode())
                color: Theme.textSub
                font.pixelSize: Design.Typography.bodyLarge
                Layout.fillWidth: true
                elide: Label.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingLarge

                Label {
                    text: qsTr("Saved %1").arg(row.format_timestamp(row.timestamp))
                    color: Theme.textHint
                    font.pixelSize: Design.Typography.body
                    elide: Label.ElideRight
                }

                Label {
                    text: qsTr("Played %1").arg(row.format_play_time(row.play_time_seconds))
                    color: Theme.textHint
                    font.pixelSize: Design.Typography.body
                    visible: row.format_play_time(row.play_time_seconds) !== ""
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Label {
                text: row.blocked_reason
                color: Theme.dangerBr
                font.pixelSize: Design.Typography.body
                visible: !row.loadable && row.blocked_reason !== ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        Loader {
            sourceComponent: row.actions
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
