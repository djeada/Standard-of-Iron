pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property var priorities: ["critical", "urgent", "info", "ambient"]

    property var queue: []
    readonly property int count: queue.length
    readonly property var current: queue.length > 0 ? queue[0] : null

    signal presented(var entry)
    signal dismissed(var entry)

    function rank(priority) {
        var index = root.priorities.indexOf(priority);
        return index < 0 ? root.priorities.length : index;
    }

    function push(priority, message, options) {
        var opts = options || {};
        var normalized = root.rank(priority) < root.priorities.length ? priority : "info";
        var channel = opts.channel || "";
        if (channel !== "") {
            var merged = root.queue.slice();
            for (var i = 0; i < merged.length; ++i) {
                if (merged[i].channel === channel) {
                    merged[i] = Object.assign({}, merged[i], {
                            "repeats": merged[i].repeats + 1,
                            "message": message,
                            "priority": root.rank(normalized) < root.rank(merged[i].priority) ? normalized : merged[i].priority
                        });
                    root.queue = root.sorted(merged);
                    return merged[i].id;
                }
            }
        }
        var entry = {
            "id": ++root.m_nextId,
            "priority": normalized,
            "message": message,
            "detail": opts.detail || "",
            "icon": opts.icon || "",
            "channel": channel,
            "sticky": opts.sticky === true,
            "repeats": 1,
            "sequence": ++root.m_sequence
        };
        root.queue = root.sorted(root.queue.concat([entry]));
        root.presented(entry);
        return entry.id;
    }

    function critical(message, options) {
        return root.push("critical", message, options);
    }

    function urgent(message, options) {
        return root.push("urgent", message, options);
    }

    function info(message, options) {
        return root.push("info", message, options);
    }

    function ambient(message, options) {
        return root.push("ambient", message, options);
    }

    function dismiss(id) {
        var remaining = [];
        var removed = null;
        for (var i = 0; i < root.queue.length; ++i) {
            if (root.queue[i].id === id)
                removed = root.queue[i];
            else
                remaining.push(root.queue[i]);
        }
        if (removed === null)
            return false;
        root.queue = remaining;
        root.dismissed(removed);
        return true;
    }

    function dismissCurrent() {
        return root.current !== null && root.dismiss(root.current.id);
    }

    function clear() {
        root.queue = [];
    }

    function sorted(entries) {
        var copy = entries.slice();
        copy.sort(function (a, b) {
                var byPriority = root.rank(a.priority) - root.rank(b.priority);
                return byPriority !== 0 ? byPriority : a.sequence - b.sequence;
            });
        return copy;
    }

    property int m_nextId: 0
    property int m_sequence: 0
}
