pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property var activities: ({
            "idle": {
                "icon": "idle",
                "label": qsTr("Idle"),
                "hint": qsTr("Standing by and awaiting orders."),
                "resource": ""
            },
            "move": {
                "icon": "move",
                "label": qsTr("Moving"),
                "hint": qsTr("Marching to the ordered position."),
                "resource": ""
            },
            "attack": {
                "icon": "attack",
                "label": qsTr("Attacking"),
                "hint": qsTr("Engaging the assigned target."),
                "resource": ""
            },
            "patrol": {
                "icon": "patrol",
                "label": qsTr("Patrolling"),
                "hint": qsTr("Walking the patrol route between its waypoints."),
                "resource": ""
            },
            "guard": {
                "icon": "guard",
                "label": qsTr("Guarding"),
                "hint": qsTr("Holding a guard post and answering threats near it."),
                "resource": ""
            },
            "hold": {
                "icon": "hold",
                "label": qsTr("Holding position"),
                "hint": qsTr("Rooted in place; it will not pursue."),
                "resource": ""
            },
            "construct": {
                "icon": "construct",
                "label": qsTr("Building"),
                "hint": qsTr("Raising a structure on the marked site."),
                "resource": ""
            },
            "repair": {
                "icon": "repair",
                "label": qsTr("Repairing"),
                "hint": qsTr("Mending a damaged structure."),
                "resource": ""
            },
            "dismantle": {
                "icon": "dismantle",
                "label": qsTr("Dismantling"),
                "hint": qsTr("Taking a structure apart."),
                "resource": ""
            },
            "chop_wood": {
                "icon": "chop_wood",
                "label": qsTr("Cutting timber"),
                "hint": qsTr("Felling a tree for wood."),
                "resource": "wood"
            },
            "mine_stone": {
                "icon": "mine_stone",
                "label": qsTr("Quarrying stone"),
                "hint": qsTr("Breaking a boulder for stone."),
                "resource": "stone"
            },
            "mine_iron": {
                "icon": "mine_iron",
                "label": qsTr("Mining iron"),
                "hint": qsTr("Working an ore deposit for iron."),
                "resource": "iron"
            },
            "auto_gather": {
                "icon": "auto_gather",
                "label": qsTr("Auto gathering"),
                "hint": qsTr("Standing order: seek out the nearest resource and keep collecting."),
                "resource": ""
            },
            "deliver": {
                "icon": "deliver",
                "label": qsTr("Delivering"),
                "hint": qsTr("Carrying its load to the barracks."),
                "resource": ""
            },
            "heal": {
                "icon": "heal",
                "label": qsTr("Healing"),
                "hint": qsTr("Tending wounded allies nearby."),
                "resource": ""
            },
            "train": {
                "icon": "train",
                "label": qsTr("Training"),
                "hint": qsTr("Producing the next unit in the queue."),
                "resource": ""
            },
            "blocked": {
                "icon": "blocked",
                "label": qsTr("Blocked"),
                "hint": qsTr("Waiting: the way is blocked or the target is gone."),
                "resource": ""
            }
        })

    readonly property var states: ({
            "active": {
                "label": qsTr("Active"),
                "hint": qsTr("Work is under way."),
                "tone": "accent",
                "decorated": false
            },
            "queued": {
                "label": qsTr("Queued"),
                "hint": qsTr("Ordered, but the unit is still on its way."),
                "tone": "secondary",
                "decorated": true
            },
            "unavailable": {
                "label": qsTr("Unavailable"),
                "hint": qsTr("The target cannot be worked; give a new order."),
                "tone": "danger",
                "decorated": true
            },
            "interrupted": {
                "label": qsTr("Interrupted"),
                "hint": qsTr("Work stopped part-way; re-issue the order to resume."),
                "tone": "warning",
                "decorated": true
            }
        })

    readonly property string defaultActivity: "idle"
    readonly property string defaultState: "active"

    function activity(activityId) {
        return root.activities[activityId] || root.activities[root.defaultActivity];
    }

    function state(stateId) {
        return root.states[stateId] || root.states[root.defaultState];
    }

    function iconFor(activityId) {
        return root.activity(activityId).icon;
    }

    function label(activityId) {
        return root.activity(activityId).label;
    }

    function hint(activityId) {
        return root.activity(activityId).hint;
    }

    function resourceFor(activityId) {
        return root.activity(activityId).resource;
    }

    function isGathering(activityId) {
        return root.resourceFor(activityId) !== "";
    }

    function summary(activityId, stateId) {
        var name = root.label(activityId);
        if (stateId === root.defaultState || stateId === undefined)
            return name;
        return qsTr("%1 — %2").arg(name).arg(root.state(stateId).label);
    }

    function description(activityId, stateId, count) {
        var text = root.summary(activityId, stateId);
        if (count !== undefined && count > 1)
            text = qsTr("%1 (%2 units)").arg(text).arg(count);
        var stateHint = root.state(stateId).hint;
        if (stateId !== root.defaultState && stateId !== undefined)
            return text + "\n" + root.hint(activityId) + "\n" + stateHint;
        return text + "\n" + root.hint(activityId);
    }

    function ids() {
        var result = [];
        for (var key in root.activities)
            result.push(key);
        return result;
    }

    function stateIds() {
        var result = [];
        for (var key in root.states)
            result.push(key);
        return result;
    }
}
