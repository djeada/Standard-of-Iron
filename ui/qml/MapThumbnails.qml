import QtQuick 2.15

Item {
    id: root

    property var previewSource: (typeof game !== "undefined" && game.setup) ? game.setup : null
    property var previewStore: (typeof map_preview_provider !== "undefined") ? map_preview_provider : null
    property var cache: ({})

    property int revision: 0
    property var queue: []

    function source_for(authored, map_path) {
        if (authored && authored !== "")
            return authored;
        if (!map_path || map_path === "")
            return "";
        var finished = root.revision;
        if (cache[map_path] !== undefined)
            return cache[map_path];
        if (!previewSource || !previewSource.map_preview || !previewStore)
            return "";
        if (queue.indexOf(map_path) < 0) {
            queue.push(map_path);
            render_timer.start();
        }
        return "";
    }

    function render_next() {
        if (queue.length === 0) {
            render_timer.stop();
            return;
        }
        var map_path = queue.shift();
        if (cache[map_path] !== undefined)
            return;
        var id = "maplist:" + map_path;
        var url = "";
        try {
            previewStore.set_preview_image(id, previewSource.map_preview(map_path, []));
            url = "image://mappreview/" + id;
        } catch (e) {
            console.warn("MapThumbnails: no preview for", map_path, e);
        }
        cache[map_path] = url;
        root.revision = root.revision + 1;
    }

    Timer {
        id: render_timer

        interval: 20
        repeat: true
        onTriggered: root.render_next()
    }

    visible: false
}
