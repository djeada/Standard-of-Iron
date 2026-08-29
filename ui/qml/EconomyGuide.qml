pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property var resourceOrder: ["gold", "food", "wood", "stone", "iron"]

    function resource_label(key) {
        switch (key) {
        case "gold":
            return qsTr("Gold");
        case "food":
            return qsTr("Food");
        case "wood":
            return qsTr("Wood");
        case "stone":
            return qsTr("Stone");
        case "iron":
            return qsTr("Iron");
        }
        return key || "";
    }

    function resource_source(key) {
        switch (key) {
        case "gold":
            return qsTr("Comes from your starting treasury, mission rewards, and selling goods at a marketplace.");
        case "food":
            return qsTr("Send a builder to reap a ripe farm or slaughter a sheep with Collect, or leave Auto Gather running. Recruiting a civilian at a home spends it.");
        case "wood":
            return qsTr("Send a builder to chop a tree with Collect, or leave Auto Gather running.");
        case "stone":
            return qsTr("Send a builder to a boulder with Collect, or leave Auto Gather running.");
        case "iron":
            return qsTr("Send a builder to an iron ore deposit with Collect, or leave Auto Gather running.");
        }
        return "";
    }

    function item_label(key) {
        switch (key) {
        case "home":
            return qsTr("Home");
        case "farm":
            return qsTr("Farm");
        case "barracks":
            return qsTr("Barracks");
        case "defense_tower":
            return qsTr("Defense Tower");
        case "marketplace":
            return qsTr("Marketplace");
        case "temple":
            return qsTr("Temple");
        case "wall_segment":
            return qsTr("Wall Segment");
        case "wall_gate":
            return qsTr("Wall Gate");
        case "catapult":
            return qsTr("Catapult");
        case "ballista":
            return qsTr("Ballista");
        case "archer":
            return qsTr("Archer");
        case "swordsman":
            return qsTr("Swordsman");
        case "spearman":
            return qsTr("Spearman");
        case "horse_swordsman":
            return qsTr("Horse Swordsman");
        case "horse_archer":
            return qsTr("Horse Archer");
        case "horse_spearman":
            return qsTr("Horse Spearman");
        case "healer":
            return qsTr("Healer");
        case "builder":
            return qsTr("Builder");
        case "civilian":
            return qsTr("Civilian");
        case "elephant":
            return qsTr("Elephant");
        }
        return key || "";
    }

    function item_purpose(key) {
        switch (key) {
        case "home":
            return qsTr("Raises civilians, who carry reserve to the nearest barracks. Each one costs food.");
        case "farm":
            return qsTr("Grows grain in cycles. A builder reaps it for food once it ripens.");
        case "barracks":
            return qsTr("Recruits troops and receives everything your builders gather.");
        case "defense_tower":
            return qsTr("Shoots at enemies that come near it.");
        case "marketplace":
            return qsTr("Buys and sells resources for gold.");
        case "temple":
            return qsTr("Watches over a wide stretch of ground, holds a settlement together, and takes in healers.");
        case "wall_segment":
            return qsTr("Blocks enemy movement.");
        case "wall_gate":
            return qsTr("Opens for your troops and allies, shut to everyone else.");
        case "catapult":
            return qsTr("Long-range siege engine, best against structures.");
        case "ballista":
            return qsTr("Precise siege engine, best against units.");
        }
        return "";
    }

    function item_labels(keys, limit) {
        var names = [];
        var count = keys ? keys.length : 0;
        var shown = limit > 0 ? Math.min(limit, count) : count;
        for (var i = 0; i < shown; ++i)
            names.push(root.item_label(keys[i]));
        var text = names.join(", ");
        if (shown < count)
            text += qsTr(", and %1 more").arg(count - shown);
        return text;
    }

    function cost_summary(costs) {
        var parts = [];
        for (var i = 0; i < root.resourceOrder.length; ++i) {
            var key = root.resourceOrder[i];
            var amount = costs && costs[key] ? costs[key] : 0;
            if (amount > 0)
                parts.push(qsTr("%1 %2").arg(amount).arg(root.resource_label(key)));
        }
        return parts.length > 0 ? parts.join(", ") : qsTr("No resource cost");
    }

    function missing_summary(missing) {
        var parts = [];
        for (var i = 0; i < root.resourceOrder.length; ++i) {
            var key = root.resourceOrder[i];
            var amount = missing && missing[key] ? missing[key] : 0;
            if (amount > 0)
                parts.push(qsTr("%1 more %2").arg(amount).arg(root.resource_label(key)));
        }
        return parts.join(", ");
    }

    function gather_state_line(entry) {
        if (!entry || !entry.gatherable)
            return "";
        var workers = entry.gathering_workers || 0;
        var carrying = entry.carrying || 0;
        if (workers <= 0 && carrying <= 0)
            return qsTr("Nobody is gathering it right now.");
        var lines = [];
        if (workers > 0)
            lines.push(qsTr("Builders gathering it: %1").arg(workers));
        if (carrying > 0)
            lines.push(qsTr("%1 being hauled to a barracks").arg(carrying));
        return lines.join(" · ");
    }

    function storage_line(entry) {
        if (!entry || !entry.gatherable)
            return qsTr("Held in your treasury; no storage limit.");
        return qsTr("Hauled to a barracks yard before it is credited. The yard looks full at %1.").arg(entry.display_cap || 0);
    }

    function deficit_line(entry) {
        if (!entry || !entry.shortfall || entry.shortfall <= 0)
            return "";
        return qsTr("Short %1 for a %2.").arg(entry.shortfall).arg(root.item_label(entry.shortfall_item));
    }

    function resource_tooltip(entry) {
        if (!entry)
            return "";
        var lines = [];
        lines.push(qsTr("%1 in store: %2").arg(root.resource_label(entry.key)).arg(entry.amount || 0));
        var source = root.resource_source(entry.key);
        if (source !== "")
            lines.push(source);
        if (entry.used_by && entry.used_by.length > 0)
            lines.push(qsTr("Spent on: %1").arg(root.item_labels(entry.used_by, 4)));
        lines.push(root.storage_line(entry));
        var gathering = root.gather_state_line(entry);
        if (gathering !== "")
            lines.push(gathering);
        var deficit = root.deficit_line(entry);
        if (deficit !== "")
            lines.push(deficit);
        return lines.join("\n");
    }

    function coach_title(step) {
        switch (step) {
        case "gather":
            return qsTr("Step 1 — Gather");
        case "build":
            return qsTr("Step 2 — Build");
        case "recruit":
            return qsTr("Step 3 — Recruit");
        case "army":
            return qsTr("Step 4 — Keep an army");
        }
        return qsTr("Your settlement runs itself");
    }

    function coach_body(step, state) {
        switch (step) {
        case "gather":
            if (!state || (state.builder_count || 0) <= 0)
                return qsTr("Recruit a builder at a barracks, then send it to collect wood, stone or iron.");
            return qsTr("Select a builder, press Collect, and click a tree, boulder, ore deposit, ripe farm or sheep. It hauls the load to a barracks yard, then goes back for more.");
        case "build":
            return qsTr("With a builder selected, press Build and place a Home to raise your reserve, or a Barracks to recruit from.");
        case "recruit":
            return qsTr("Select a barracks and recruit troops. Each one costs reserve and resources, both shown on its card.");
        case "army":
            return qsTr("Keep recruiting while your builders gather. Raise Homes and Farms when reserve or food runs out.");
        }
        return qsTr("Gathering, building and recruiting are all under way.");
    }
}
