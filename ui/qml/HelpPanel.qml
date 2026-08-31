import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Item {
    id: root

    property bool from_menu: false
    property int current_tab: 0

    signal close_requested
    signal start_tutorial_requested

    readonly property var tutorial: (typeof game !== 'undefined' && game && game.tutorial) ? game.tutorial : null
    readonly property bool tutorial_active: tutorial !== null && tutorial.active

    readonly property var tabs: [qsTr("Basics"), qsTr("Economy"), qsTr("Buildings"), qsTr("Army"), qsTr("Commander"), qsTr("Camera"), qsTr("Controls"), qsTr("Tutorial")]

    readonly property var basics_sections: [{
            "heading": qsTr("Selecting"),
            "body": qsTr("Left-click a unit to select it, or hold the left button and drag a box around several. Hold Ctrl to add to the selection. Selected units show a ring at their feet, and the panel at the bottom lists them by type - click a type to keep only those.")
        }, {
            "heading": qsTr("Giving orders"),
            "body": qsTr("Right-click the ground to move, right-click an enemy to attack. Hold the right button and drag before releasing to choose the way the troops will face: one unit turns to face where you drag, several units are placed in formation facing it. The command grid offers Attack, Guard, Patrol, Hold, Stop and the builder and commander orders. A marker shows where troops are going and the banner above the grid confirms each order - or explains why it was refused.")
        }, {
            "heading": qsTr("Guard - hold a spot"),
            "body": qsTr("Press Guard, then left-click the ground the troops should hold. They fight anything that comes within about 10 m of that spot, let go of anything that runs past it, and walk back once the fight is over. Press Guard again, or give a move or attack order, to release them; Stop alone will not lift it. Any soldier can guard.")
        }, {
            "heading": qsTr("Hold - stand fast"),
            "body": qsTr("Press Hold and the troops kneel where they stand. They never take a step, but they reach further, hit harder and take more punishment than they would on the move, and braced spears gut a cavalry charge. That is the trade: pursuit for staying power. Press Hold again, or give a move, attack or Stop order, to stand them up. Only archers and spearmen can hold.")
        }, {
            "heading": qsTr("Patrol - walk a beat"),
            "body": qsTr("Press Patrol, left-click the first waypoint, then left-click the second. The troops march between the two points for good and attack whatever crosses the line - useful for watching a flank while you look elsewhere. Right-click cancels while you are setting it; any other order, or Stop, clears the route.")
        }, {
            "heading": qsTr("Winning and losing"),
            "body": qsTr("Each mission states its own victory and defeat conditions in the Objectives screen (Escape, then Objectives). In every battle your commander must survive: a nation dies with the man who leads it, and a lone commander is already lost.")
        }]

    readonly property var economy_sections: [{
            "heading": qsTr("The three materials"),
            "body": qsTr("Wood comes from pine trees, stone from boulders and iron from ore seams. A builder harvests a node with Collect (or Auto Gather), then carries the load to the stone yard beside a friendly barracks. Only when it is dropped there does the counter in the top bar rise. The yard's piles grow and shrink with your stores.")
        }, {
            "heading": qsTr("Food"),
            "body": qsTr("Food feeds your settlement: every civilian recruited at a Home costs it. A Farm grows grain in cycles; when the field turns gold a builder reaps it with Collect and hauls the grain home, and the field is sown again. Builders can also slaughter a sheep for a smaller haul. Auto Gather set to food keeps a builder working the farms.")
        }, {
            "heading": qsTr("Gold and trade"),
            "body": qsTr("Gold comes with the camp and cannot be mined. A marketplace lets you buy materials for gold and sell surplus for it; a temple costs gold to raise.")
        }, {
            "heading": qsTr("Costs"),
            "body": qsTr("Every card - recruit or building - lists its price. A grey card means something is short; hover it to read what. Homes cost 50 wood and 15 stone, a farm 40 wood and 10 stone, a barracks 100 wood and 60 stone, a defence tower 60 wood and 80 stone; soldiers cost wood and iron plus reserve, and civilians cost food.")
        }]

    readonly property var buildings_sections: [{
            "heading": qsTr("Placing a structure"),
            "body": qsTr("Select a builder and press Build to open the structure list. Move the outline over the ground: green means flat, clear ground; red means it is blocked by another building, water or a slope. Scroll to rotate, left-click to confirm, right-click to cancel. The builder walks over and works until it stands.")
        }, {
            "heading": qsTr("What each building does"),
            "body": qsTr("Barracks recruit soldiers and hold your stockpile yard. Homes raise families and civilians. Farms grow grain that builders reap for food. Defence towers shoot at anything in range. Marketplaces trade. Temples strengthen morale and take in healers. Walls and gates shape the field; gates open for your own troops.")
        }, {
            "heading": qsTr("Repair and dismantle"),
            "body": qsTr("A damaged building can be repaired by a builder with the Repair order. A building you no longer need can be taken down with the Dismantle order: pick your builders, press Dismantle and click the building. Part of what it cost comes back once the work is done, and calling the crew off before then pays nothing.")
        }]

    readonly property var army_sections: [{
            "heading": qsTr("Recruiting"),
            "body": qsTr("Left-click a barracks and pick a soldier; healers take their vows at a temple instead. Recruits queue up to five deep and march out to the rally flag. Each costs reserve from the building's own pool as well as materials.")
        }, {
            "heading": qsTr("Reserve"),
            "body": qsTr("A barracks or temple starts with a reserve and spends it on every recruit; the card shows exactly what each one costs. Homes refill it: each Home raises families over time, a civilian can be recruited there for food and sent to either building with Deliver, and that building's reserve grows. A squad then stands in the field for what it cost, which is what the top bar counts against the map's manpower cap.")
        }, {
            "heading": qsTr("Formations and lines"),
            "body": qsTr("Spearmen hold a line, archers punish from behind it, cavalry breaks flanks, healers mend nearby wounded on their own. Right-click and drag with several units selected to deploy them in a formation that faces the drag; the Formation order opens the same planner where they stand. Run trades stamina for speed.")
        }]

    readonly property var commander_sections: [{
            "heading": qsTr("The standard"),
            "body": qsTr("Your commander carries the standard. Troops close to him fight with higher morale, and morale decides who breaks first. Keep him behind the spears: if he falls the mission is lost.")
        }, {
            "heading": qsTr("Aura and Rally"),
            "body": qsTr("With the commander selected, Aura empowers his troops for a stretch and then has to recharge - each commander brings his own bonus, radius and timings, and the button prints them along with the time left. A glow marks every soldier inside the ring, the ring travels with him, and a wounded commander cannot call it. Rally plants a flag that your army marches to - useful for pulling a scattered force back into a line.")
        }, {
            "heading": qsTr("Taking direct control"),
            "body": qsTr("Where the mission allows it, you can step into the commander's boots and lead from the field. The controls for that mode are listed under Controls.")
        }]

    readonly property var camera_sections: CameraGuide.entries.map(function (entry) {
            return {
                "heading": entry.state.length > 0 ? entry.name + " — " + entry.state : entry.name,
                "body": entry.control + ": " + entry.detail
            };
        })

    readonly property var flow_sections: [{
            "heading": qsTr("Pace and view"),
            "body": qsTr("The top-left buttons pause the battle and set the speed, from half up to quadruple; Space pauses too, and + and - step through the speeds. The active speed stays lit while paused and can be changed there. Move the camera with the arrow keys or WASD, or push the mouse to the screen edge; scroll or PgUp and PgDown to zoom, Q and E to rotate, Ctrl with the up and down arrows to tilt; Home returns to your camp and Follow keeps the camera on your selection. Every one of these can be rebound under Settings › Controls, where each camera command says what it does.")
        }]

    function sections_for(index) {
        switch (index) {
        case 0:
            return basics_sections;
        case 1:
            return economy_sections;
        case 2:
            return buildings_sections;
        case 3:
            return army_sections;
        case 4:
            return commander_sections;
        case 5:
            return camera_sections;
        default:
            return [];
        }
    }

    function binding_groups() {
        var actions = InputBindings.actions || [];
        var groups = {};
        var order = [];
        for (var i = 0; i < actions.length; ++i) {
            var a = actions[i];
            var key = a.category || a.context || "";
            if (!groups[key]) {
                groups[key] = [];
                order.push(key);
            }
            groups[key].push(a);
        }
        var result = [];
        for (var j = 0; j < order.length; ++j)
            result.push({
                    "heading": order[j],
                    "entries": groups[order[j]]
                });
        return result;
    }

    anchors.fill: parent
    z: 10
    focus: true
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.close_requested();
            event.accepted = true;
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.scrim
    }

    Design.IronPanel {
        id: container

        width: Math.min(parent.width * 0.78, Design.Metrics.space24 * 38)
        height: Math.min(parent.height * 0.86, Design.Metrics.space24 * 30)
        anchors.centerIn: parent
        raised: true
        accessibleName: qsTr("Field manual")

        ColumnLayout {
            anchors.fill: parent
            spacing: Design.Metrics.space12

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Field Manual")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.title
                    font.weight: Design.Typography.bold
                }

                Design.IronIconButton {
                    iconText: Design.Icons.close
                    tooltip: qsTr("Close the field manual")
                    onClicked: root.close_requested()
                }
            }

            Design.IronTabBar {
                id: tabBar

                Layout.fillWidth: true
                tabs: root.tabs
                currentIndex: root.current_tab
                onCurrentIndexChanged: root.current_tab = currentIndex
            }

            ScrollView {
                id: scroller

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                Column {
                    width: scroller.availableWidth
                    spacing: Design.Metrics.space16

                    Repeater {
                        model: root.current_tab <= 5 ? root.sections_for(root.current_tab).concat(root.current_tab === 0 ? root.flow_sections : []) : []

                        delegate: Column {
                            required property var modelData

                            width: scroller.availableWidth
                            spacing: Design.Metrics.space4

                            Text {
                                width: parent.width
                                text: modelData.heading
                                color: Design.Theme.accent
                                font.family: Design.Typography.displayFamily
                                font.pixelSize: Design.Typography.heading
                                font.weight: Design.Typography.bold
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                width: parent.width
                                text: modelData.body
                                color: Design.Theme.textPrimary
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.body
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Repeater {
                        model: root.current_tab === 6 ? root.binding_groups() : []

                        delegate: Column {
                            id: bindingGroup

                            required property var modelData

                            width: scroller.availableWidth
                            spacing: Design.Metrics.space4

                            Text {
                                width: parent.width
                                text: bindingGroup.modelData.heading
                                color: Design.Theme.accent
                                font.family: Design.Typography.displayFamily
                                font.pixelSize: Design.Typography.heading
                                font.weight: Design.Typography.bold
                            }

                            Repeater {
                                model: bindingGroup.modelData.entries

                                delegate: RowLayout {
                                    required property var modelData

                                    width: bindingGroup.width
                                    spacing: Design.Metrics.space12

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: Design.Theme.textPrimary
                                        font.family: Design.Typography.family
                                        font.pixelSize: Design.Typography.body
                                        elide: Text.ElideRight
                                    }

                                    Design.IronBadge {
                                        text: modelData.unbound ? qsTr("unbound") : modelData.displayShortcut
                                        tone: modelData.unbound ? Design.Theme.textDisabled : Design.Theme.accent
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        width: scroller.availableWidth
                        spacing: Design.Metrics.space8
                        visible: root.current_tab === 7

                        Text {
                            width: parent.width
                            text: qsTr("The tutorial is a guided first battle on a small meadow. Each step names one goal, tells you when it is done, and explains why an order is refused. It also points: the button a step is talking about lights up in the command bar, and what you are meant to act on is ringed on the field and pinned on the minimap - press Show me to swing the camera onto it. Steps can be paused, skipped and replayed at any time.")
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.body
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: root.tutorial ? root.tutorial.steps : []

                            delegate: Design.IronObjectiveRow {
                                required property var modelData
                                required property int index

                                width: scroller.availableWidth
                                objectiveText: qsTr("%1. %2").arg(index + 1).arg(modelData.title)
                                detail: modelData.objective
                                objectiveState: modelData.state === "complete" ? "complete" : (modelData.state === "active" ? "active" : "optional")
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space8

                Design.IronButton {
                    visible: root.from_menu && !root.tutorial_active
                    text: qsTr("Start the tutorial")
                    tone: "primary"
                    onClicked: root.start_tutorial_requested()
                }

                Design.IronButton {
                    visible: root.tutorial_active
                    text: qsTr("Show tutorial card")
                    onClicked: {
                        root.tutorial.set_visible(true);
                        root.close_requested();
                    }
                }

                Design.IronButton {
                    visible: root.tutorial_active
                    text: qsTr("Restart the steps")
                    tone: "destructive"
                    onClicked: {
                        root.tutorial.restart();
                        root.close_requested();
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Design.IronButton {
                    text: qsTr("Close")
                    tone: "primary"
                    onClicked: root.close_requested()
                }
            }
        }
    }
}
