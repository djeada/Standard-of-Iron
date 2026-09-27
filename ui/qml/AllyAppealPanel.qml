import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

Column {
    id: root

    property var engine: (typeof game !== 'undefined') ? game : null
    property bool gate: true
    readonly property bool showing: root.gate && appeals.count > 0

    function open_appeal(appeal) {
        for (var i = 0; i < appeals.count; ++i) {
            if (appeals.get(i).appealId === appeal.id)
                return;
        }
        appeals.append({
                "appealId": appeal.id,
                "name": appeal.name || "",
                "kind": appeal.kind || "resources",
                "text": appeal.text || "",
                "acceptLabel": appeal.accept || qsTr("Accept"),
                "declineLabel": appeal.decline || qsTr("Refuse"),
                "targetX": Number(appeal.x) || 0,
                "targetZ": Number(appeal.z) || 0,
                "seconds": Number(appeal.seconds) || 45
            });
    }

    function close_appeal(appealId) {
        for (var i = appeals.count - 1; i >= 0; --i) {
            if (appeals.get(i).appealId === appealId)
                appeals.remove(i);
        }
    }

    function answer(appealId, accept) {
        if (!root.engine || !root.engine.production || !root.engine.production.answer_ally_appeal)
            return;
        if (root.engine.production.answer_ally_appeal(appealId, accept))
            root.close_appeal(appealId);
    }

    width: Design.Metrics.space24 * 16
    spacing: Design.Metrics.space8
    visible: root.showing

    ListModel {
        id: appeals
    }

    Connections {
        function onAlly_appeal_opened(appeal) {
            root.open_appeal(appeal);
        }

        function onAlly_appeal_closed(appealId) {
            root.close_appeal(appealId);
        }

        target: root.engine
    }

    Repeater {
        model: appeals

        delegate: Design.IronPanel {
            id: card

            required property int index
            required property var appealId
            required property string name
            required property string kind
            required property string text
            required property string acceptLabel
            required property string declineLabel
            required property real targetX
            required property real targetZ
            required property real seconds

            property real remaining: card.seconds

            objectName: "allyAppealCard"
            width: root.width
            raised: true
            accessibleName: card.text
            implicitHeight: cardLayout.implicitHeight + Design.Metrics.space12 * 2

            Timer {
                interval: 1000
                repeat: true
                running: card.remaining > 0
                onTriggered: card.remaining = Math.max(0, card.remaining - 1)
            }

            ColumnLayout {
                id: cardLayout

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: Design.Metrics.space4

                Text {
                    Layout.fillWidth: true
                    text: card.kind === "resources" ? qsTr("%1 asks for supplies").arg(card.name) : (card.kind === "defend" ? qsTr("%1 calls for help").arg(card.name) : qsTr("%1 calls you to arms").arg(card.name))
                    color: Design.Theme.accent
                    elide: Text.ElideRight
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                    font.weight: Design.Typography.bold
                }

                Text {
                    Layout.fillWidth: true
                    text: card.text
                    color: Design.Theme.textSecondary
                    wrapMode: Text.WordWrap
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 3
                    radius: 1
                    color: Design.Theme.borderStrong

                    Rectangle {
                        width: parent.width * (card.seconds > 0 ? card.remaining / card.seconds : 0)
                        height: parent.height
                        radius: parent.radius
                        color: Design.Theme.accent
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Design.Metrics.space8

                    Design.IronButton {
                        objectName: "allyAppealAccept"
                        text: card.acceptLabel
                        tone: "primary"
                        onClicked: root.answer(card.appealId, true)
                    }

                    Design.IronButton {
                        objectName: "allyAppealDecline"
                        text: card.declineLabel
                        onClicked: root.answer(card.appealId, false)
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Design.IronButton {
                        objectName: "allyAppealShow"
                        visible: card.kind !== "resources"
                        text: qsTr("Show")
                        onClicked: {
                            if (root.engine && root.engine.camera && root.engine.camera.look_at_world)
                                root.engine.camera.look_at_world(card.targetX, card.targetZ);
                        }
                    }
                }
            }
        }
    }
}
