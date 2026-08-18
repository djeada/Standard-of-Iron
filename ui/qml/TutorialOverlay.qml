import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var tutorial: (typeof game !== 'undefined' && game && game.tutorial) ? game.tutorial : null
    property bool game_is_paused: false

    property int max_height: 10000

    readonly property bool active: tutorial !== null && tutorial.active
    readonly property bool collapsed: tutorial !== null && !tutorial.visible
    readonly property int panelWidth: Math.min(Design.Metrics.space24 * 17, Math.max(Design.Metrics.space24 * 12, parent ? parent.width * 0.3 : 360))

    signal pause_requested
    signal help_requested

    readonly property bool can_show_target: tutorial !== null && tutorial.has_focus_point

    function look_at_focus() {
        if (!root.can_show_target || typeof game === 'undefined' || !game.camera || !game.camera.look_at_world)
            return;
        var points = root.tutorial.focus_points;
        if (!points || points.length === 0)
            return;
        var sum_x = 0;
        var sum_z = 0;
        for (var i = 0; i < points.length; ++i) {
            sum_x += points[i].world_x || 0;
            sum_z += points[i].world_z || 0;
        }
        game.camera.look_at_world(sum_x / points.length, sum_z / points.length);
    }

    visible: active
    implicitWidth: panelWidth
    implicitHeight: collapsed ? collapsedBar.implicitHeight : Math.min(card.implicitHeight, max_height)
    width: implicitWidth
    height: implicitHeight

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Tutorial")

    Design.IronPanel {
        id: collapsedBar

        anchors.left: parent.left
        anchors.right: parent.right
        visible: root.collapsed
        raised: true
        implicitHeight: collapsedRow.implicitHeight + Design.Metrics.space12 * 2

        RowLayout {
            id: collapsedRow

            anchors.fill: parent
            spacing: Design.Metrics.space8

            Text {
                text: Design.Icons.objective
                color: Design.Theme.accent
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
            }

            Text {
                Layout.fillWidth: true
                text: root.tutorial ? qsTr("Tutorial %1/%2 · %3").arg(root.tutorial.step_index + 1).arg(root.tutorial.step_count).arg(root.tutorial.title) : ""
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                elide: Text.ElideRight
            }

            Design.IronButton {
                text: qsTr("Show")
                tone: "primary"
                implicitWidth: Math.max(72, contentItem.implicitWidth + Design.Metrics.space16)
                onClicked: root.tutorial.set_visible(true)
            }
        }
    }

    Design.IronPanel {
        id: card

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: !root.collapsed
        raised: true
        border.color: root.tutorial && root.tutorial.step_complete ? Design.Theme.success : Design.Theme.borderStrong
        implicitHeight: column.implicitHeight + Design.Metrics.space12 * 2
        accessibleName: qsTr("Tutorial step")

        Flickable {
            id: cardScroller

            anchors.fill: parent
            contentWidth: width
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ScrollBar.vertical: ScrollBar {
                policy: cardScroller.contentHeight > cardScroller.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            ColumnLayout {
                id: column

                width: cardScroller.width
                spacing: Design.Metrics.space8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Design.Metrics.space8

                    Text {
                        text: Design.Icons.objective
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.body
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.tutorial ? qsTr("TUTORIAL · STEP %1 OF %2").arg(root.tutorial.step_index + 1).arg(root.tutorial.step_count) : ""
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                        elide: Text.ElideRight
                    }

                    Design.IronIconButton {
                        iconText: "?"
                        tooltip: qsTr("Open the help screen")
                        implicitHeight: Design.Metrics.toolControlHeight
                        onClicked: root.help_requested()
                    }

                    Design.IronIconButton {
                        iconText: Design.Icons.disclosureOpen
                        tooltip: qsTr("Hide the tutorial card (it can be shown again)")
                        implicitHeight: Design.Metrics.toolControlHeight
                        onClicked: root.tutorial.set_visible(false)
                    }
                }

                Design.IronProgressBar {
                    Layout.fillWidth: true
                    value: root.tutorial ? (root.tutorial.step_index + (root.tutorial.step_complete ? 1 : 0)) / Math.max(1, root.tutorial.step_count) : 0
                    fillColor: Design.Theme.accent
                }

                Text {
                    Layout.fillWidth: true
                    text: root.tutorial ? root.tutorial.title : ""
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.heading
                    font.weight: Design.Typography.bold
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: root.tutorial ? root.tutorial.body : ""
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    wrapMode: Text.WordWrap
                }

                Design.IronObjectiveRow {
                    Layout.fillWidth: true
                    objectiveText: root.tutorial ? root.tutorial.objective : ""
                    objectiveState: root.tutorial ? root.tutorial.objective_state : "active"
                    detail: root.tutorial ? root.tutorial.progress_text : ""
                    progress: root.tutorial ? root.tutorial.progress : -1
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.tutorial && root.tutorial.hint !== "" && !root.tutorial.step_complete
                    implicitHeight: hintRow.implicitHeight + Design.Metrics.space8 * 2
                    radius: Design.Metrics.radiusSmall
                    color: Design.Theme.panelIron
                    border.width: Design.Metrics.borderThin
                    border.color: Design.Theme.warning

                    RowLayout {
                        id: hintRow

                        anchors.fill: parent
                        anchors.margins: Design.Metrics.space8
                        spacing: Design.Metrics.space8

                        Text {
                            Layout.alignment: Qt.AlignTop
                            text: Design.Icons.warning
                            color: Design.Theme.warning
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.body
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.tutorial ? root.tutorial.hint : ""
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.label
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.tutorial && root.tutorial.step_complete
                    text: root.tutorial && root.tutorial.step_index + 1 >= root.tutorial.step_count ? qsTr("Well done - the tutorial is complete.") : qsTr("Well done. The next step follows in a moment, or press Continue.")
                    color: Design.Theme.success
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                    font.weight: Design.Typography.medium
                    wrapMode: Text.WordWrap
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Design.Metrics.space8

                    Design.IronButton {
                        text: root.game_is_paused ? qsTr("Resume") : qsTr("Pause")
                        tone: root.game_is_paused ? "primary" : "secondary"
                        implicitWidth: Math.max(88, contentItem.implicitWidth + Design.Metrics.space16)
                        onClicked: root.pause_requested()
                    }

                    Design.IronButton {
                        text: qsTr("Show me")
                        tone: "secondary"
                        implicitWidth: Math.max(88, contentItem.implicitWidth + Design.Metrics.space16)
                        visible: root.can_show_target && !root.tutorial.step_complete
                        onClicked: root.look_at_focus()
                    }

                    Design.IronButton {
                        text: qsTr("Replay step")
                        implicitWidth: Math.max(88, contentItem.implicitWidth + Design.Metrics.space16)
                        onClicked: root.tutorial.replay_step()
                    }

                    Design.IronButton {
                        text: qsTr("Skip step")
                        implicitWidth: Math.max(88, contentItem.implicitWidth + Design.Metrics.space16)
                        visible: root.tutorial && !root.tutorial.step_complete
                        onClicked: root.tutorial.skip_step()
                    }

                    Design.IronButton {
                        text: qsTr("Continue")
                        tone: "primary"
                        implicitWidth: Math.max(88, contentItem.implicitWidth + Design.Metrics.space16)
                        visible: root.tutorial && root.tutorial.step_complete
                        onClicked: root.tutorial.continue_step()
                    }

                    Design.IronButton {
                        text: qsTr("End tutorial")
                        tone: "destructive"
                        implicitWidth: Math.max(88, contentItem.implicitWidth + Design.Metrics.space16)
                        onClicked: root.tutorial.stop()
                    }
                }
            }
        }
    }
}
