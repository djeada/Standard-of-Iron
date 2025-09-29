#include "map_loader.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace Game::Map {

static bool readGrid(const QJsonObject& obj, GridDefinition& grid) {
    if (obj.contains("width")) grid.width = obj.value("width").toInt(grid.width);
    if (obj.contains("height")) grid.height = obj.value("height").toInt(grid.height);
    if (obj.contains("tileSize")) grid.tileSize = float(obj.value("tileSize").toDouble(grid.tileSize));
    return grid.width > 0 && grid.height > 0 && grid.tileSize > 0.0f;
}

static bool readCamera(const QJsonObject& obj, CameraDefinition& cam) {
    if (obj.contains("center")) {
        auto arr = obj.value("center").toArray();
        if (arr.size() == 3) {
            cam.center = {float(arr[0].toDouble(0.0)), float(arr[1].toDouble(0.0)), float(arr[2].toDouble(0.0))};
        }
    }
    if (obj.contains("distance")) cam.distance = float(obj.value("distance").toDouble(cam.distance));
    if (obj.contains("tiltDeg")) cam.tiltDeg = float(obj.value("tiltDeg").toDouble(cam.tiltDeg));
    if (obj.contains("fovY")) cam.fovY = float(obj.value("fovY").toDouble(cam.fovY));
    if (obj.contains("near")) cam.nearPlane = float(obj.value("near").toDouble(cam.nearPlane));
    if (obj.contains("far")) cam.farPlane = float(obj.value("far").toDouble(cam.farPlane));
    return true;
}

static void readSpawns(const QJsonArray& arr, std::vector<UnitSpawn>& out) {
    out.clear();
    out.reserve(arr.size());
    for (const auto& v : arr) {
        auto o = v.toObject();
        UnitSpawn s;
        s.type = o.value("type").toString();
        s.x = float(o.value("x").toDouble(0.0));
        s.z = float(o.value("z").toDouble(0.0));
        s.playerId = o.value("playerId").toInt(0);
        out.push_back(s);
    }
}

bool MapLoader::loadFromJsonFile(const QString& path, MapDefinition& outMap, QString* outError) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QString("Failed to open map file: %1").arg(path);
        return false;
    }
    auto data = f.readAll();
    f.close();

    QJsonParseError perr;
    auto doc = QJsonDocument::fromJson(data, &perr);
    if (perr.error != QJsonParseError::NoError) {
        if (outError) *outError = QString("JSON parse error at %1: %2").arg(perr.offset).arg(perr.errorString());
        return false;
    }
    if (!doc.isObject()) {
        if (outError) *outError = "Map JSON root must be an object";
        return false;
    }
    auto root = doc.object();

    outMap.name = root.value("name").toString("Unnamed Map");

    if (root.contains("grid") && root.value("grid").isObject()) {
        if (!readGrid(root.value("grid").toObject(), outMap.grid)) {
            if (outError) *outError = "Invalid grid definition";
            return false;
        }
    }

    if (root.contains("camera") && root.value("camera").isObject()) {
        readCamera(root.value("camera").toObject(), outMap.camera);
    }

    if (root.contains("spawns") && root.value("spawns").isArray()) {
        readSpawns(root.value("spawns").toArray(), outMap.spawns);
    }

    return true;
}

} // namespace Game::Map
