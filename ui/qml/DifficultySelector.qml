import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

FocusScope {
    id: root

    property var presets: (typeof game !== "undefined" && game.setup) ? game.setup.difficulty_presets : []
    property string selected_id: DifficultyCatalog.defaultId
    property bool compact: false
    property bool show_heading: true

    signal chosen(string difficulty_id)

    readonly property var entries: DifficultyCatalog.entries
    readonly property int card_width: root.compact ? Math.max(260, Math.floor((column.width - 8) / 2)) : 168
    readonly property int card_height: root.compact ? 46 : 158

    function index_of(difficultyId) {
        for (let i = 0; i < root.entries.length; i++) {
            if (root.entries[i].id === difficultyId)
                return i;
        }
        return 1;
    }

    function select_index(index) {
        let clamped = Math.max(0, Math.min(root.entries.length - 1, index));
        let next = root.entries[clamped].id;
        if (next === root.selected_id)
            return;
        root.selected_id = next;
        Design.UiSound.activate();
        root.chosen(next);
    }

    function step(delta) {
        root.select_index(root.index_of(root.selected_id) + delta);
    }

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    Keys.onLeftPressed: root.step(-1)
    Keys.onRightPressed: root.step(1)
    Keys.onUpPressed: root.step(-1)
    Keys.onDownPressed: root.step(1)

    Column {
        id: column

        width: parent.width
        spacing: 8

        Row {
            visible: root.show_heading
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: DifficultyCatalog.label
                color: Design.Theme.textPrimary
                font.pixelSize: Design.Typography.body
                font.bold: true
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: DifficultyCatalog.scopeNote
                color: Design.Theme.textSecondary
                font.pixelSize: Design.Typography.caption
                width: Math.max(0, column.width - 120)
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }
        }

        Flow {
            width: parent.width
            spacing: 8

            Repeater {
                model: root.entries

                delegate: Rectangle {
                    id: card

                    required property int index
                    required property var modelData

                    readonly property bool selected: root.selected_id === card.modelData.id
                    readonly property bool focused: root.activeFocus && card.selected
                    readonly property string summary_text: DifficultyCatalog.summary_for(root.presets, card.modelData.id)

                    width: root.card_width
                    height: root.card_height
                    radius: 8
                    color: card.selected ? Qt.lighter(Design.Theme.panelIron, 1.35) : Design.Theme.panelIron
                    border.width: card.selected || card.focused ? 2 : 1
                    border.color: card.selected ? card.modelData.accent : (hover.containsMouse ? Design.Theme.borderStrong : Design.Theme.borderSubtle)

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: card.modelData.name
                    Accessible.description: card.summary_text
                    Accessible.checked: card.selected

                    Behavior on border.color  {
                        ColorAnimation {
                            duration: Design.Motion.fast
                        }
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4
                        visible: !root.compact

                        Design.IronVectorIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            iconId: card.modelData.icon
                            accent: card.modelData.accent
                            width: Design.Metrics.iconMedium * 2
                            height: width
                            opacity: card.selected ? 1 : 0.82
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: card.modelData.name
                            color: card.selected ? card.modelData.accent : Design.Theme.textPrimary
                            font.pixelSize: Design.Typography.label
                            font.bold: true
                        }

                        Text {
                            width: parent.width
                            text: card.summary_text
                            color: Design.Theme.textSecondary
                            font.pixelSize: Design.Typography.caption
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 22
                        anchors.topMargin: 4
                        anchors.bottomMargin: 4
                        spacing: 8
                        visible: root.compact

                        Design.IronVectorIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            iconId: card.modelData.icon
                            accent: card.modelData.accent
                            width: Design.Metrics.iconMedium
                            height: width
                            opacity: card.selected ? 1 : 0.82
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, parent.width - Design.Metrics.iconMedium - 8)
                            spacing: 1

                            Text {
                                text: card.modelData.name
                                color: card.selected ? card.modelData.accent : Design.Theme.textPrimary
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            Text {
                                width: parent.width
                                text: card.summary_text
                                color: Design.Theme.textSecondary
                                font.pixelSize: Design.Typography.caption
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 6
                        width: 14
                        height: 14
                        radius: 7
                        visible: card.selected
                        color: card.modelData.accent

                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            color: Design.Theme.backgroundDeep
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: hover

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onContainsMouseChanged: {
                            if (containsMouse)
                                Design.UiSound.hover();
                        }
                        onClicked: {
                            root.forceActiveFocus();
                            root.select_index(card.index);
                        }
                    }

                    ToolTip {
                        visible: hover.containsMouse || card.focused
                        delay: hover.containsMouse ? 400 : 0
                        text: card.modelData.quip + "\n" + card.summary_text
                    }
                }
            }
        }
    }
}
