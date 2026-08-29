import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property string unit_type: ""
    property string nation: ""
    property var profile: ({})
    property var order_states: ({})
    property bool show_availability: false

    readonly property bool has_profile: !!root.profile && root.profile.valid === true
    readonly property var role_tags: root.profile && root.profile.role_tags ? root.profile.role_tags : []
    readonly property var special_abilities: root.profile && root.profile.abilities ? root.profile.abilities : []

    signal close_requested

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    function load(unitType, nationId) {
        root.unit_type = unitType || "";
        root.nation = nationId || "";
        if (!root.game_ready() || !game.activity || !game.activity.unit_profile) {
            root.profile = ({});
            return;
        }
        root.profile = game.activity.unit_profile(root.unit_type, root.nation);
        root.order_states = game.orders && game.orders.action_states ? game.orders.action_states() : ({});
    }

    function stat(key, fallback) {
        if (!root.profile || root.profile[key] === undefined)
            return fallback;
        return root.profile[key];
    }

    function number(key, digits) {
        var value = root.stat(key, 0);
        return Number(value).toFixed(digits === undefined ? 1 : digits);
    }

    readonly property var order_rows: [{
            "id": "attack",
            "label": qsTr("Attack")
        }, {
            "id": "guard",
            "label": qsTr("Guard")
        }, {
            "id": "hold",
            "label": qsTr("Hold")
        }, {
            "id": "patrol",
            "label": qsTr("Patrol")
        }, {
            "id": "heal",
            "label": qsTr("Medic")
        }, {
            "id": "build",
            "label": qsTr("Build")
        }, {
            "id": "collect",
            "label": qsTr("Collect")
        }, {
            "id": "repair",
            "label": qsTr("Repair")
        }]

    function order_available(orderId) {
        if (!root.order_states || !root.order_states[orderId])
            return false;
        return root.order_states[orderId].enabled === true;
    }

    function order_eligible(orderId) {
        if (!root.order_states || !root.order_states[orderId])
            return false;
        return (root.order_states[orderId].eligibleCount || 0) > 0;
    }

    readonly property var available_orders: {
        if (!root.show_availability)
            return [];
        var rows = [];
        for (var i = 0; i < root.order_rows.length; ++i) {
            if (root.order_eligible(root.order_rows[i].id))
                rows.push(root.order_rows[i]);
        }
        return rows;
    }

    anchors.fill: parent
    z: 40
    focus: visible
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.close_requested();
            event.accepted = true;
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.close_requested()
    }

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.scrim
    }

    Design.IronPanel {
        id: container

        objectName: "unitInspectCard"

        width: Math.min(parent.width * 0.68, Design.Metrics.space24 * 30)
        height: Math.min(parent.height * 0.84, Design.Metrics.space24 * 28)
        anchors.centerIn: parent
        raised: true
        accessibleName: qsTr("Unit details")

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Design.Metrics.space8

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space12

                Rectangle {
                    Layout.preferredWidth: Design.Metrics.space24 * 3
                    Layout.preferredHeight: Design.Metrics.space24 * 3
                    radius: Design.Metrics.radiusSmall
                    color: Design.Theme.backgroundDeep
                    border.width: Design.Metrics.borderThin
                    border.color: Design.Theme.borderStrong

                    Image {
                        anchors.fill: parent
                        anchors.margins: Design.Metrics.space4
                        fillMode: Image.PreserveAspectFit
                        source: Design.Icons.unit(root.unit_type, root.nation)
                        smooth: true
                        mipmap: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.Metrics.space4

                    Text {
                        objectName: "unitInspectName"
                        Layout.fillWidth: true
                        text: root.stat("display_name", root.unit_type)
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.displayFamily
                        font.pixelSize: Design.Typography.heading
                        font.weight: Design.Typography.bold
                        elide: Text.ElideRight
                    }

                    Text {
                        objectName: "unitInspectRole"
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: root.stat("role", "")
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.label
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.Metrics.space4

                        Repeater {
                            model: root.role_tags

                            delegate: Design.IronBadge {
                                required property var modelData

                                tone: Design.Theme.borderStrong
                                text: modelData
                            }
                        }
                    }
                }

                Design.IronIconButton {
                    Layout.alignment: Qt.AlignTop
                    iconText: Design.Icons.close
                    tooltip: qsTr("Close the unit details")
                    onClicked: root.close_requested()
                }
            }

            Design.IronDivider {
                Layout.fillWidth: true
            }

            ScrollView {
                objectName: "unitInspectScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: container.width - Design.Metrics.space24 * 2
                    spacing: Design.Metrics.space12

                    Text {
                        Layout.fillWidth: true
                        visible: !root.has_profile
                        text: qsTr("No details are available for this unit.")
                        color: Design.Theme.textDisabled
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.label
                        wrapMode: Text.WordWrap
                    }

                    Design.IronInspectorSection {
                        objectName: "unitInspectCombat"
                        Layout.fillWidth: true
                        visible: root.has_profile
                        title: qsTr("In the field")

                        Grid {
                            width: parent.width
                            columns: width > Design.Metrics.space24 * 16 ? 3 : 2
                            spacing: Design.Metrics.space8

                            Repeater {
                                model: [{
                                        "label": qsTr("Attack"),
                                        "value": root.stat("attack_damage", 0)
                                    }, {
                                        "label": qsTr("Damage per second"),
                                        "value": root.number("damage_per_second", 1)
                                    }, {
                                        "label": qsTr("Range"),
                                        "value": root.number("attack_range", 1)
                                    }, {
                                        "label": qsTr("Health"),
                                        "value": root.stat("health", 0)
                                    }, {
                                        "label": qsTr("Speed"),
                                        "value": root.number("speed", 1)
                                    }, {
                                        "label": qsTr("Sight"),
                                        "value": root.number("vision_range", 1)
                                    }]

                                delegate: Column {
                                    required property var modelData

                                    width: (parent.width - parent.spacing * (parent.columns - 1)) / parent.columns
                                    spacing: Design.Metrics.space2

                                    Text {
                                        width: parent.width
                                        text: parent.modelData.label
                                        color: Design.Theme.textSecondary
                                        font.family: Design.Typography.family
                                        font.pixelSize: Design.Typography.caption
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: parent.width
                                        text: parent.modelData.value
                                        color: Design.Theme.textPrimary
                                        font.family: Design.Typography.family
                                        font.pixelSize: Design.Typography.bodyLarge
                                        font.weight: Design.Typography.bold
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    Design.IronInspectorSection {
                        objectName: "unitInspectAbilities"
                        Layout.fillWidth: true
                        visible: root.has_profile && (root.special_abilities.length > 0 || root.available_orders.length > 0)
                        title: qsTr("What it can do")

                        Repeater {
                            model: root.special_abilities

                            delegate: Column {
                                required property var modelData

                                width: parent.width
                                spacing: Design.Metrics.space2

                                Text {
                                    width: parent.width
                                    text: parent.modelData.name
                                    color: Design.Theme.accent
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.label
                                    font.weight: Design.Typography.bold
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    width: parent.width
                                    visible: text !== ""
                                    text: parent.modelData.effect
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        Flow {
                            width: parent.width
                            spacing: Design.Metrics.space4
                            visible: root.available_orders.length > 0

                            Repeater {
                                model: root.available_orders

                                delegate: Design.IronBadge {
                                    required property var modelData

                                    tone: root.order_available(modelData.id) ? Design.Theme.success : Design.Theme.textDisabled
                                    text: modelData.label
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            visible: root.available_orders.length > 0
                            text: qsTr("Highlighted orders are the ones this selection can carry out right now.")
                            color: Design.Theme.textDisabled
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            wrapMode: Text.WordWrap
                        }
                    }

                    Design.IronInspectorSection {
                        objectName: "unitInspectCost"
                        Layout.fillWidth: true
                        visible: root.has_profile && root.stat("cost", 0) > 0
                        title: qsTr("What it costs")

                        Text {
                            width: parent.width
                            text: qsTr("Reserve %1  ·  Build time %2s").arg(root.stat("cost", 0)).arg(root.number("build_time", 0))
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.label
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            width: parent.width
                            text: EconomyGuide.cost_summary(root.stat("resource_costs", ({})))
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.label
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            width: parent.width
                            visible: root.stat("individuals_per_unit", 1) > 1
                            text: qsTr("Fields %n soldier(s) in one squad.", "", root.stat("individuals_per_unit", 1))
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            wrapMode: Text.WordWrap
                        }
                    }

                    Design.IronInspectorSection {
                        objectName: "unitInspectTactics"
                        Layout.fillWidth: true
                        visible: root.has_profile && (root.stat("strengths", "") !== "" || root.stat("weaknesses", "") !== "")
                        title: qsTr("Strengths and weaknesses")

                        Repeater {
                            model: [{
                                    "glyph": Design.Icons.attack,
                                    "tone": Design.Theme.success,
                                    "body": root.stat("strengths", "")
                                }, {
                                    "glyph": Design.Icons.warning,
                                    "tone": Design.Theme.warning,
                                    "body": root.stat("weaknesses", "")
                                }]

                            delegate: Row {
                                required property var modelData

                                width: parent.width
                                spacing: Design.Metrics.space8
                                visible: modelData.body !== ""

                                Text {
                                    text: parent.modelData.glyph
                                    color: parent.modelData.tone
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.label
                                }

                                Text {
                                    width: parent.width - Design.Metrics.space24
                                    text: parent.modelData.body
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.label
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    Design.IronInspectorSection {
                        objectName: "unitInspectHistory"
                        Layout.fillWidth: true
                        visible: root.has_profile && root.stat("history", "") !== ""
                        title: qsTr("History")

                        Text {
                            width: parent.width
                            text: root.stat("history", "")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.label
                            wrapMode: Text.WordWrap
                            font.italic: true
                        }
                    }
                }
            }
        }
    }
}
