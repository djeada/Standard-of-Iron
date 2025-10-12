#include "map_loader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cstdint>

namespace Game::Map {

static bool readGrid(const QJsonObject &obj, GridDefinition &grid) {
  if (obj.contains("width"))
    grid.width = obj.value("width").toInt(grid.width);
  if (obj.contains("height"))
    grid.height = obj.value("height").toInt(grid.height);
  if (obj.contains("tileSize"))
    grid.tileSize = float(obj.value("tileSize").toDouble(grid.tileSize));
  return grid.width > 0 && grid.height > 0 && grid.tileSize > 0.0f;
}

static bool readCamera(const QJsonObject &obj, CameraDefinition &cam) {
  if (obj.contains("center")) {
    auto arr = obj.value("center").toArray();
    if (arr.size() == 3) {
      cam.center = {float(arr[0].toDouble(0.0)), float(arr[1].toDouble(0.0)),
                    float(arr[2].toDouble(0.0))};
    }
  }
  if (obj.contains("distance"))
    cam.distance = float(obj.value("distance").toDouble(cam.distance));
  if (obj.contains("tiltDeg"))
    cam.tiltDeg = float(obj.value("tiltDeg").toDouble(cam.tiltDeg));
  if (obj.contains("fovY"))
    cam.fovY = float(obj.value("fovY").toDouble(cam.fovY));
  if (obj.contains("near"))
    cam.nearPlane = float(obj.value("near").toDouble(cam.nearPlane));
  if (obj.contains("far"))
    cam.farPlane = float(obj.value("far").toDouble(cam.farPlane));
  if (obj.contains("yaw") || obj.contains("yawDeg")) {

    const QString k = obj.contains("yaw") ? "yaw" : "yawDeg";
    cam.yawDeg = float(obj.value(k).toDouble(cam.yawDeg));
  }
  return true;
}

static QVector3D readVector3(const QJsonValue &value,
                             const QVector3D &fallback) {
  if (!value.isArray())
    return fallback;
  auto arr = value.toArray();
  if (arr.size() != 3)
    return fallback;
  return QVector3D(float(arr[0].toDouble(fallback.x())),
                   float(arr[1].toDouble(fallback.y())),
                   float(arr[2].toDouble(fallback.z())));
}

static void readBiome(const QJsonObject &obj, BiomeSettings &out) {
  if (obj.contains("seed"))
    out.seed = static_cast<std::uint32_t>(
        std::max(0.0, obj.value("seed").toDouble(out.seed)));
  if (obj.contains("patchDensity"))
    out.patchDensity =
        float(obj.value("patchDensity").toDouble(out.patchDensity));
  if (obj.contains("patchJitter"))
    out.patchJitter = float(obj.value("patchJitter").toDouble(out.patchJitter));
  if (obj.contains("bladeHeight")) {
    auto arr = obj.value("bladeHeight").toArray();
    if (arr.size() == 2) {
      out.bladeHeightMin = float(arr[0].toDouble(out.bladeHeightMin));
      out.bladeHeightMax = float(arr[1].toDouble(out.bladeHeightMax));
    }
  }
  if (obj.contains("bladeWidth")) {
    auto arr = obj.value("bladeWidth").toArray();
    if (arr.size() == 2) {
      out.bladeWidthMin = float(arr[0].toDouble(out.bladeWidthMin));
      out.bladeWidthMax = float(arr[1].toDouble(out.bladeWidthMax));
    }
  }
  if (obj.contains("backgroundBladeDensity"))
    out.backgroundBladeDensity =
        float(obj.value("backgroundBladeDensity")
                  .toDouble(out.backgroundBladeDensity));
  if (obj.contains("swayStrength"))
    out.swayStrength =
        float(obj.value("swayStrength").toDouble(out.swayStrength));
  if (obj.contains("swaySpeed"))
    out.swaySpeed = float(obj.value("swaySpeed").toDouble(out.swaySpeed));
  if (obj.contains("heightNoise")) {
    auto arr = obj.value("heightNoise").toArray();
    if (arr.size() == 2) {
      out.heightNoiseAmplitude =
          float(arr[0].toDouble(out.heightNoiseAmplitude));
      out.heightNoiseFrequency =
          float(arr[1].toDouble(out.heightNoiseFrequency));
    }
  }

  if (obj.contains("grassPrimary"))
    out.grassPrimary = readVector3(obj.value("grassPrimary"), out.grassPrimary);
  if (obj.contains("grassSecondary"))
    out.grassSecondary =
        readVector3(obj.value("grassSecondary"), out.grassSecondary);
  if (obj.contains("grassDry"))
    out.grassDry = readVector3(obj.value("grassDry"), out.grassDry);
  if (obj.contains("soilColor"))
    out.soilColor = readVector3(obj.value("soilColor"), out.soilColor);
  if (obj.contains("rockLow"))
    out.rockLow = readVector3(obj.value("rockLow"), out.rockLow);
  if (obj.contains("rockHigh"))
    out.rockHigh = readVector3(obj.value("rockHigh"), out.rockHigh);
  if (obj.contains("terrainMacroNoiseScale"))
    out.terrainMacroNoiseScale =
        float(obj.value("terrainMacroNoiseScale")
                  .toDouble(out.terrainMacroNoiseScale));
  if (obj.contains("terrainDetailNoiseScale"))
    out.terrainDetailNoiseScale =
        float(obj.value("terrainDetailNoiseScale")
                  .toDouble(out.terrainDetailNoiseScale));
  if (obj.contains("terrainSoilHeight"))
    out.terrainSoilHeight =
        float(obj.value("terrainSoilHeight").toDouble(out.terrainSoilHeight));
  if (obj.contains("terrainSoilSharpness"))
    out.terrainSoilSharpness = float(
        obj.value("terrainSoilSharpness").toDouble(out.terrainSoilSharpness));
  if (obj.contains("terrainRockThreshold"))
    out.terrainRockThreshold = float(
        obj.value("terrainRockThreshold").toDouble(out.terrainRockThreshold));
  if (obj.contains("terrainRockSharpness"))
    out.terrainRockSharpness = float(
        obj.value("terrainRockSharpness").toDouble(out.terrainRockSharpness));
  if (obj.contains("terrainAmbientBoost"))
    out.terrainAmbientBoost = float(
        obj.value("terrainAmbientBoost").toDouble(out.terrainAmbientBoost));
  if (obj.contains("terrainRockDetailStrength"))
    out.terrainRockDetailStrength =
        float(obj.value("terrainRockDetailStrength")
                  .toDouble(out.terrainRockDetailStrength));
  if (obj.contains("backgroundSwayVariance"))
    out.backgroundSwayVariance =
        float(obj.value("backgroundSwayVariance")
                  .toDouble(out.backgroundSwayVariance));
  if (obj.contains("backgroundScatterRadius"))
    out.backgroundScatterRadius =
        float(obj.value("backgroundScatterRadius")
                  .toDouble(out.backgroundScatterRadius));
}

static void readVictoryConfig(const QJsonObject &obj, VictoryConfig &out) {

  if (obj.contains("type")) {
    out.victoryType = obj.value("type").toString("elimination");
  }

  if (obj.contains("key_structures") && obj.value("key_structures").isArray()) {
    out.keyStructures.clear();
    auto arr = obj.value("key_structures").toArray();
    for (const auto &v : arr) {
      out.keyStructures.push_back(v.toString());
    }
  }

  if (obj.contains("duration")) {
    out.surviveTimeDuration = float(obj.value("duration").toDouble(0.0));
  }

  if (obj.contains("defeat_conditions") &&
      obj.value("defeat_conditions").isArray()) {
    out.defeatConditions.clear();
    auto arr = obj.value("defeat_conditions").toArray();
    for (const auto &v : arr) {
      out.defeatConditions.push_back(v.toString());
    }
  }
}

static void readSpawns(const QJsonArray &arr, std::vector<UnitSpawn> &out) {
  out.clear();
  out.reserve(arr.size());
  for (const auto &v : arr) {
    auto o = v.toObject();
    UnitSpawn s;
    s.type = o.value("type").toString();
    s.x = float(o.value("x").toDouble(0.0));
    s.z = float(o.value("z").toDouble(0.0));
    s.playerId = o.value("playerId").toInt(0);
    out.push_back(s);
  }
}

static void readTerrain(const QJsonArray &arr, std::vector<TerrainFeature> &out,
                        const GridDefinition &grid, CoordSystem coordSys) {
  out.clear();
  out.reserve(arr.size());

  for (const auto &v : arr) {
    auto o = v.toObject();
    TerrainFeature feature;

    QString typeStr = o.value("type").toString("flat").toLower();
    if (typeStr == "mountain") {
      feature.type = TerrainType::Mountain;
    } else if (typeStr == "hill") {
      feature.type = TerrainType::Hill;
    } else {
      feature.type = TerrainType::Flat;
    }

    float x = float(o.value("x").toDouble(0.0));
    float z = float(o.value("z").toDouble(0.0));

    if (coordSys == CoordSystem::Grid) {
      const float tile = std::max(0.0001f, grid.tileSize);
      feature.centerX = (x - (grid.width * 0.5f - 0.5f)) * tile;
      feature.centerZ = (z - (grid.height * 0.5f - 0.5f)) * tile;
    } else {
      feature.centerX = x;
      feature.centerZ = z;
    }

    feature.radius = float(o.value("radius").toDouble(5.0));
    feature.width = float(o.value("width").toDouble(0.0));
    feature.depth = float(o.value("depth").toDouble(0.0));

    if (feature.width == 0.0f && feature.depth == 0.0f) {
      feature.width = feature.radius;
      feature.depth = feature.radius;
    }

    feature.height = float(o.value("height").toDouble(2.0));
    feature.rotationDeg = float(o.value("rotation").toDouble(0.0));

    if (o.contains("entrances") && o.value("entrances").isArray()) {
      auto entranceArr = o.value("entrances").toArray();
      for (const auto &e : entranceArr) {
        auto eObj = e.toObject();
        float ex = float(eObj.value("x").toDouble(0.0));
        float ez = float(eObj.value("z").toDouble(0.0));

        feature.entrances.push_back(QVector3D(ex, 0.0f, ez));
      }
    }

    out.push_back(feature);
  }
}

bool MapLoader::loadFromJsonFile(const QString &path, MapDefinition &outMap,
                                 QString *outError) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (outError)
      *outError = QString("Failed to open map file: %1").arg(path);
    return false;
  }
  auto data = f.readAll();
  f.close();

  QJsonParseError perr;
  auto doc = QJsonDocument::fromJson(data, &perr);
  if (perr.error != QJsonParseError::NoError) {
    if (outError)
      *outError = QString("JSON parse error at %1: %2")
                      .arg(perr.offset)
                      .arg(perr.errorString());
    return false;
  }
  if (!doc.isObject()) {
    if (outError)
      *outError = "Map JSON root must be an object";
    return false;
  }
  auto root = doc.object();

  outMap.name = root.value("name").toString("Unnamed Map");

  if (root.contains("coordSystem")) {
    const QString cs = root.value("coordSystem").toString().trimmed().toLower();
    if (cs == "world")
      outMap.coordSystem = CoordSystem::World;
    else
      outMap.coordSystem = CoordSystem::Grid;
  }

  if (root.contains("maxTroopsPerPlayer")) {
    outMap.maxTroopsPerPlayer = root.value("maxTroopsPerPlayer").toInt(50);
  }

  if (root.contains("grid") && root.value("grid").isObject()) {
    if (!readGrid(root.value("grid").toObject(), outMap.grid)) {
      if (outError)
        *outError = "Invalid grid definition";
      return false;
    }
  }

  if (root.contains("camera") && root.value("camera").isObject()) {
    readCamera(root.value("camera").toObject(), outMap.camera);
  }

  if (root.contains("spawns") && root.value("spawns").isArray()) {
    readSpawns(root.value("spawns").toArray(), outMap.spawns);
  }

  if (root.contains("terrain") && root.value("terrain").isArray()) {
    readTerrain(root.value("terrain").toArray(), outMap.terrain, outMap.grid,
                outMap.coordSystem);
  }

  if (root.contains("biome") && root.value("biome").isObject()) {
    readBiome(root.value("biome").toObject(), outMap.biome);
  }

  if (root.contains("victory") && root.value("victory").isObject()) {
    readVictoryConfig(root.value("victory").toObject(), outMap.victory);
  }

  return true;
}

} // namespace Game::Map
