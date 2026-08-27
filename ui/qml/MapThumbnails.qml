import QtQuick 2.15

Item {
    id: root

    property var previewSource: (typeof game !== "undefined" && game.setup) ? game.setup : null
    property var previewStore: (typeof map_preview_provider !== "undefined") ? map_preview_provider : null
    property var cache: ({})

    function source_for(authored, map_path) {
        if (authored && authored !== "")
            return authored;
        if (!map_path || map_path === "")
            return "";
        if (cache[map_path] !== undefined)
            return cache[map_path];
        if (!previewSource || !previewSource.map_preview || !previewStore)
            return "";
        var id = "maplist:" + map_path;
        var url = "";
        try {
            previewStore.set_preview_image(id, previewSource.map_preview(map_path, []));
            url = "image://mappreview/" + id;
        } catch (e) {
            console.warn("MapThumbnails: no preview for", map_path, e);
        }
        cache[map_path] = url;
        return url;
    }

    visible: false
}
