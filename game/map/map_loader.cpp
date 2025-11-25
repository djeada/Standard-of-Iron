#include "map_loader.h"
#include "json_keys.h"
#include "map/map_definition.h"
#include "map/terrain.h"
#include "units/spawn_type.h"

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <algorithm>
#include <cstdint>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <vector>

namespace Game::Map {

using namespace JsonKeys;

namespace {

auto readGrid(const QJsonObject &obj, GridDefinition &grid) -> bool {
  if (obj.contains(WIDTH)) {
    grid.width = obj.value(WIDTH).toInt(grid.width);
  }
  if (obj.contains(HEIGHT)) {
    grid.height = obj.value(HEIGHT).toInt(grid.height);
  }
  if (obj.contains(TILE_SIZE)) {
    grid.tile_size = float(obj.value(TILE_SIZE).toDouble(grid.tile_size));
  }
  return grid.width > 0 && grid.height > 0 && grid.tile_size > 0.0F;
}

auto readCamera(const QJsonObject &obj, CameraDefinition &cam) -> bool {
  if (obj.contains(CENTER)) {
    auto arr = obj.value(CENTER).toArray();
    if (arr.size() == 3) {
      cam.center = {float(arr[0].toDouble(0.0)), float(arr[1].toDouble(0.0)),
                    float(arr[2].toDouble(0.0))};
    }
  }
  if (obj.contains(DISTANCE)) {
    cam.distance = float(obj.value(DISTANCE).toDouble(cam.distance));
  }
  if (obj.contains(TILT_DEG)) {
    cam.tiltDeg = float(obj.value(TILT_DEG).toDouble(cam.tiltDeg));
  }
  if (obj.contains(FOV_Y)) {
    cam.fovY = float(obj.value(FOV_Y).toDouble(cam.fovY));
  }
  if (obj.contains(NEAR)) {
    cam.near_plane = float(obj.value(NEAR).toDouble(cam.near_plane));
  }
  if (obj.contains(FAR)) {
    cam.far_plane = float(obj.value(FAR).toDouble(cam.far_plane));
  }
  if (obj.contains(YAW) || obj.contains("yaw_deg")) {

    const QString yaw_key = obj.contains(YAW) ? YAW : "yaw_deg";
    cam.yaw_deg = float(obj.value(yaw_key).toDouble(cam.yaw_deg));
  }
  return true;
}

auto readVector3(const QJsonValue &value,
                 const QVector3D &fallback) -> QVector3D {
  if (!value.isArray()) {
    return fallback;
  }
  auto arr = value.toArray();
  if (arr.size() != 3) {
    return fallback;
  }
  return {float(arr[0].toDouble(fallback.x())),
          float(arr[1].toDouble(fallback.y())),
          float(arr[2].toDouble(fallback.z()))};
}

void readBiome(const QJsonObject &obj, BiomeSettings &out) {
  // First, check for ground_type and apply defaults if specified
  if (obj.contains(GROUND_TYPE)) {
    const QString ground_type_str = obj.value(GROUND_TYPE).toString();
    GroundType parsed_ground_type;
    if (tryParseGroundType(ground_type_str, parsed_ground_type)) {
      applyGroundTypeDefaults(out, parsed_ground_type);
    } else {
      qWarning() << "MapLoader: unknown ground type" << ground_type_str
                 << "- using default (forest_mud)";
      applyGroundTypeDefaults(out, GroundType::ForestMud);
    }
  }
  // Then apply any explicit overrides from JSON
  if (obj.contains(SEED)) {
    out.seed = static_cast<std::uint32_t>(
        std::max(0.0, obj.value(SEED).toDouble(out.seed)));
  }
  if (obj.contains(PATCH_DENSITY)) {
    out.patchDensity =
        float(obj.value(PATCH_DENSITY).toDouble(out.patchDensity));
  }
  if (obj.contains(PATCH_JITTER)) {
    out.patchJitter = float(obj.value(PATCH_JITTER).toDouble(out.patchJitter));
  }
  if (obj.contains(BLADE_HEIGHT)) {
    auto arr = obj.value(BLADE_HEIGHT).toArray();
    if (arr.size() == 2) {
      out.bladeHeightMin = float(arr[0].toDouble(out.bladeHeightMin));
      out.bladeHeightMax = float(arr[1].toDouble(out.bladeHeightMax));
    }
  }
  if (obj.contains(BLADE_WIDTH)) {
    auto arr = obj.value(BLADE_WIDTH).toArray();
    if (arr.size() == 2) {
      out.bladeWidthMin = float(arr[0].toDouble(out.bladeWidthMin));
      out.bladeWidthMax = float(arr[1].toDouble(out.bladeWidthMax));
    }
  }
  if (obj.contains(BACKGROUND_BLADE_DENSITY)) {
    out.backgroundBladeDensity =
        float(obj.value(BACKGROUND_BLADE_DENSITY)
                  .toDouble(out.backgroundBladeDensity));
  }
  if (obj.contains(SWAY_STRENGTH)) {
    out.sway_strength =
        float(obj.value(SWAY_STRENGTH).toDouble(out.sway_strength));
  }
  if (obj.contains(SWAY_SPEED)) {
    out.sway_speed = float(obj.value(SWAY_SPEED).toDouble(out.sway_speed));
  }
  if (obj.contains(HEIGHT_NOISE)) {
    auto arr = obj.value(HEIGHT_NOISE).toArray();
    if (arr.size() == 2) {
      out.heightNoiseAmplitude =
          float(arr[0].toDouble(out.heightNoiseAmplitude));
      out.heightNoiseFrequency =
          float(arr[1].toDouble(out.heightNoiseFrequency));
    }
  }

  if (obj.contains(GRASS_PRIMARY)) {
    out.grassPrimary = readVector3(obj.value(GRASS_PRIMARY), out.grassPrimary);
  }
  if (obj.contains(GRASS_SECONDARY)) {
    out.grassSecondary =
        readVector3(obj.value(GRASS_SECONDARY), out.grassSecondary);
  }
  if (obj.contains(GRASS_DRY)) {
    out.grassDry = readVector3(obj.value(GRASS_DRY), out.grassDry);
  }
  if (obj.contains(SOIL_COLOR)) {
    out.soilColor = readVector3(obj.value(SOIL_COLOR), out.soilColor);
  }
  if (obj.contains(ROCK_LOW)) {
    out.rockLow = readVector3(obj.value(ROCK_LOW), out.rockLow);
  }
  if (obj.contains(ROCK_HIGH)) {
    out.rockHigh = readVector3(obj.value(ROCK_HIGH), out.rockHigh);
  }
  if (obj.contains(TERRAIN_MACRO_NOISE_SCALE)) {
    out.terrainMacroNoiseScale =
        float(obj.value(TERRAIN_MACRO_NOISE_SCALE)
                  .toDouble(out.terrainMacroNoiseScale));
  }
  if (obj.contains(TERRAIN_DETAIL_NOISE_SCALE)) {
    out.terrainDetailNoiseScale =
        float(obj.value(TERRAIN_DETAIL_NOISE_SCALE)
                  .toDouble(out.terrainDetailNoiseScale));
  }
  if (obj.contains(TERRAIN_SOIL_HEIGHT)) {
    out.terrainSoilHeight =
        float(obj.value(TERRAIN_SOIL_HEIGHT).toDouble(out.terrainSoilHeight));
  }
  if (obj.contains(TERRAIN_SOIL_SHARPNESS)) {
    out.terrainSoilSharpness = float(
        obj.value(TERRAIN_SOIL_SHARPNESS).toDouble(out.terrainSoilSharpness));
  }
  if (obj.contains(TERRAIN_ROCK_THRESHOLD)) {
    out.terrainRockThreshold = float(
        obj.value(TERRAIN_ROCK_THRESHOLD).toDouble(out.terrainRockThreshold));
  }
  if (obj.contains(TERRAIN_ROCK_SHARPNESS)) {
    out.terrainRockSharpness = float(
        obj.value(TERRAIN_ROCK_SHARPNESS).toDouble(out.terrainRockSharpness));
  }
  if (obj.contains(TERRAIN_AMBIENT_BOOST)) {
    out.terrainAmbientBoost = float(
        obj.value(TERRAIN_AMBIENT_BOOST).toDouble(out.terrainAmbientBoost));
  }
  if (obj.contains(TERRAIN_ROCK_DETAIL_STRENGTH)) {
    out.terrainRockDetailStrength =
        float(obj.value(TERRAIN_ROCK_DETAIL_STRENGTH)
                  .toDouble(out.terrainRockDetailStrength));
  }
  if (obj.contains(BACKGROUND_SWAY_VARIANCE)) {
    out.backgroundSwayVariance =
        float(obj.value(BACKGROUND_SWAY_VARIANCE)
                  .toDouble(out.backgroundSwayVariance));
  }
  if (obj.contains(BACKGROUND_SCATTER_RADIUS)) {
    out.backgroundScatterRadius =
        float(obj.value(BACKGROUND_SCATTER_RADIUS)
                  .toDouble(out.backgroundScatterRadius));
  }
  if (obj.contains(PLANT_DENSITY)) {
    out.plant_density =
        float(obj.value(PLANT_DENSITY).toDouble(out.plant_density));
  }
  if (obj.contains(GROUND_IRREGULARITY_ENABLED)) {
    out.groundIrregularityEnabled = obj.value(GROUND_IRREGULARITY_ENABLED)
                                        .toBool(out.groundIrregularityEnabled);
  }
  if (obj.contains(IRREGULARITY_SCALE)) {
    out.irregularityScale =
        float(obj.value(IRREGULARITY_SCALE).toDouble(out.irregularityScale));
  }
  if (obj.contains(IRREGULARITY_AMPLITUDE)) {
    out.irregularityAmplitude = float(
        obj.value(IRREGULARITY_AMPLITUDE).toDouble(out.irregularityAmplitude));
  }
}

void readVictoryConfig(const QJsonObject &obj, VictoryConfig &out) {

  if (obj.contains("type")) {
    out.victoryType = obj.value("type").toString("elimination");
  }

  if (obj.contains("key_structures") && obj.value("key_structures").isArray()) {
    out.keyStructures.clear();
    auto arr = obj.value("key_structures").toArray();
    for (const auto &val : arr) {
      out.keyStructures.push_back(val.toString());
    }
  }

  if (obj.contains("duration")) {
    out.surviveTimeDuration = float(obj.value("duration").toDouble(0.0));
  }

  if (obj.contains("defeat_conditions") &&
      obj.value("defeat_conditions").isArray()) {
    out.defeatConditions.clear();
    auto arr = obj.value("defeat_conditions").toArray();
    for (const auto &val : arr) {
      out.defeatConditions.push_back(val.toString());
    }
  }
}

void readSpawns(const QJsonArray &arr, std::vector<UnitSpawn> &out) {
  out.clear();
  out.reserve(arr.size());
  for (const auto &spawn_val : arr) {
    auto spawn_obj = spawn_val.toObject();
    UnitSpawn spawn;
    const QString type_str = spawn_obj.value(TYPE).toString();
    if (!Game::Units::tryParseSpawnType(type_str, spawn.type)) {
      qWarning() << "MapLoader: unknown spawn type" << type_str << "- skipping";
      continue;
    }
    spawn.x = float(spawn_obj.value(X).toDouble(0.0));
    spawn.z = float(spawn_obj.value(Z).toDouble(0.0));

    if (spawn_obj.contains(PLAYER_ID) && !spawn_obj.value(PLAYER_ID).isNull()) {
      spawn.player_id = spawn_obj.value(PLAYER_ID).toInt(0);
    } else {
      spawn.player_id = -1;
    }

    spawn.team_id = spawn_obj.value(TEAM_ID).toInt(0);
    constexpr int default_max_population = 100;
    spawn.maxPopulation =
        spawn_obj.value(MAX_POPULATION).toInt(default_max_population);

    if (spawn_obj.contains(NATION)) {
      const QString nation_str = spawn_obj.value(NATION).toString();
      if (Game::Systems::NationID parsed_nation_id;
          Game::Systems::tryParseNationID(nation_str, parsed_nation_id)) {
        spawn.nation = parsed_nation_id;
      } else {
        qWarning() << "MapLoader: unknown nation" << nation_str
                   << "- will use default";
      }
    }

    out.push_back(spawn);
  }
}

void readFireCamps(const QJsonArray &arr, std::vector<FireCamp> &out) {
  out.clear();
  out.reserve(arr.size());
  for (const auto &camp_val : arr) {
    auto camp_obj = camp_val.toObject();
    FireCamp fire_camp;
    fire_camp.x = float(camp_obj.value("x").toDouble(0.0));
    fire_camp.z = float(camp_obj.value("z").toDouble(0.0));
    fire_camp.intensity = float(camp_obj.value("intensity").toDouble(1.0));
    constexpr double default_firecamp_radius = 3.0;
    fire_camp.radius =
        float(camp_obj.value("radius").toDouble(default_firecamp_radius));
    fire_camp.persistent = camp_obj.value("persistent").toBool(true);
    out.push_back(fire_camp);
  }
}

void readTerrain(const QJsonArray &arr, std::vector<TerrainFeature> &out,
                 const GridDefinition &grid, CoordSystem coordSys) {
  out.clear();
  out.reserve(arr.size());

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr double default_terrain_radius = 5.0;
  constexpr double default_terrain_height = 2.0;

  for (const auto &terrain_val : arr) {
    auto terrain_obj = terrain_val.toObject();
    TerrainFeature feature;

    const QString type_str = terrain_obj.value("type").toString("flat");
    if (!tryParseTerrainType(type_str, feature.type)) {
      qWarning() << "MapLoader: unknown terrain type" << type_str
                 << "- defaulting to flat";
      feature.type = TerrainType::Flat;
    }

    const float coord_x = float(terrain_obj.value("x").toDouble(0.0));
    const float coord_z = float(terrain_obj.value("z").toDouble(0.0));

    if (coordSys == CoordSystem::Grid) {
      const float tile = std::max(min_tile_size, grid.tile_size);
      feature.center_x =
          (coord_x - (grid.width * grid_center_offset - grid_center_offset)) *
          tile;
      feature.center_z =
          (coord_z - (grid.height * grid_center_offset - grid_center_offset)) *
          tile;
    } else {
      feature.center_x = coord_x;
      feature.center_z = coord_z;
    }

    feature.radius =
        float(terrain_obj.value("radius").toDouble(default_terrain_radius));
    feature.width = float(terrain_obj.value("width").toDouble(0.0));
    feature.depth = float(terrain_obj.value("depth").toDouble(0.0));

    if (feature.width == 0.0F && feature.depth == 0.0F) {
      feature.width = feature.radius;
      feature.depth = feature.radius;
    }

    feature.height =
        float(terrain_obj.value("height").toDouble(default_terrain_height));
    feature.rotationDeg = float(terrain_obj.value("rotation").toDouble(0.0));

    if (terrain_obj.contains("entrances") &&
        terrain_obj.value("entrances").isArray()) {
      auto entrance_arr = terrain_obj.value("entrances").toArray();
      for (const auto &entrance_val : entrance_arr) {
        auto entrance_obj = entrance_val.toObject();
        const float entrance_x = float(entrance_obj.value("x").toDouble(0.0));
        const float entrance_z = float(entrance_obj.value("z").toDouble(0.0));

        feature.entrances.emplace_back(entrance_x, 0.0F, entrance_z);
      }
    }

    out.push_back(feature);
  }
}

void readRivers(const QJsonArray &arr, std::vector<RiverSegment> &out,
                const GridDefinition &grid, CoordSystem coordSys) {
  out.clear();
  out.reserve(arr.size());

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr double default_river_width = 2.0;

  for (const auto &river_val : arr) {
    auto river_obj = river_val.toObject();
    RiverSegment segment;

    if (river_obj.contains("start") && river_obj.value("start").isArray()) {
      auto start_arr = river_obj.value("start").toArray();
      if (start_arr.size() >= 2) {
        const float start_x = float(start_arr[0].toDouble(0.0));
        const float start_z = float(start_arr[1].toDouble(0.0));

        if (coordSys == CoordSystem::Grid) {
          const float tile = std::max(min_tile_size, grid.tile_size);
          segment.start.setX((start_x - (grid.width * grid_center_offset -
                                         grid_center_offset)) *
                             tile);
          segment.start.setY(0.0F);
          segment.start.setZ((start_z - (grid.height * grid_center_offset -
                                         grid_center_offset)) *
                             tile);
        } else {
          segment.start = QVector3D(start_x, 0.0F, start_z);
        }
      }
    }

    if (river_obj.contains("end") && river_obj.value("end").isArray()) {
      auto end_arr = river_obj.value("end").toArray();
      if (end_arr.size() >= 2) {
        const float end_x = float(end_arr[0].toDouble(0.0));
        const float end_z = float(end_arr[1].toDouble(0.0));

        if (coordSys == CoordSystem::Grid) {
          const float tile = std::max(min_tile_size, grid.tile_size);
          segment.end.setX(
              (end_x - (grid.width * grid_center_offset - grid_center_offset)) *
              tile);
          segment.end.setY(0.0F);
          segment.end.setZ((end_z - (grid.height * grid_center_offset -
                                     grid_center_offset)) *
                           tile);
        } else {
          segment.end = QVector3D(end_x, 0.0F, end_z);
        }
      }
    }

    if (river_obj.contains("width")) {
      segment.width =
          float(river_obj.value("width").toDouble(default_river_width));
    }

    out.push_back(segment);
  }
}

void readBridges(const QJsonArray &arr, std::vector<Bridge> &out,
                 const GridDefinition &grid, CoordSystem coordSys) {
  out.clear();
  out.reserve(arr.size());

  constexpr float bridge_y_offset = 0.2F;
  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr double default_bridge_width = 3.0;
  constexpr double default_bridge_height = 0.5;

  for (const auto &bridge_val : arr) {
    auto bridge_obj = bridge_val.toObject();
    Bridge bridge;

    if (bridge_obj.contains("start") && bridge_obj.value("start").isArray()) {
      auto start_arr = bridge_obj.value("start").toArray();
      if (start_arr.size() >= 2) {
        const float start_x = float(start_arr[0].toDouble(0.0));
        const float start_z = float(start_arr[1].toDouble(0.0));

        if (coordSys == CoordSystem::Grid) {
          const float tile = std::max(min_tile_size, grid.tile_size);
          bridge.start.setX((start_x - (grid.width * grid_center_offset -
                                        grid_center_offset)) *
                            tile);
          bridge.start.setY(bridge_y_offset);
          bridge.start.setZ((start_z - (grid.height * grid_center_offset -
                                        grid_center_offset)) *
                            tile);
        } else {
          bridge.start = QVector3D(start_x, bridge_y_offset, start_z);
        }
      }
    }

    if (bridge_obj.contains("end") && bridge_obj.value("end").isArray()) {
      auto end_arr = bridge_obj.value("end").toArray();
      if (end_arr.size() >= 2) {
        const float end_x = float(end_arr[0].toDouble(0.0));
        const float end_z = float(end_arr[1].toDouble(0.0));

        if (coordSys == CoordSystem::Grid) {
          const float tile = std::max(min_tile_size, grid.tile_size);
          bridge.end.setX(
              (end_x - (grid.width * grid_center_offset - grid_center_offset)) *
              tile);
          bridge.end.setY(bridge_y_offset);
          bridge.end.setZ((end_z - (grid.height * grid_center_offset -
                                    grid_center_offset)) *
                          tile);
        } else {
          bridge.end = QVector3D(end_x, bridge_y_offset, end_z);
        }
      }
    }

    if (bridge_obj.contains("width")) {
      bridge.width =
          float(bridge_obj.value("width").toDouble(default_bridge_width));
    }

    if (bridge_obj.contains("height")) {
      bridge.height =
          float(bridge_obj.value("height").toDouble(default_bridge_height));
    }

    out.push_back(bridge);
  }
}

} // namespace

auto MapLoader::loadFromJsonFile(const QString &path, MapDefinition &outMap,
                                 QString *out_error) -> bool {
  QFile map_file(path);
  if (!map_file.open(QIODevice::ReadOnly)) {
    if (out_error != nullptr) {
      *out_error = QString("Failed to open map file: %1").arg(path);
    }
    return false;
  }
  auto data = map_file.readAll();
  map_file.close();

  QJsonParseError perr;
  auto doc = QJsonDocument::fromJson(data, &perr);
  if (perr.error != QJsonParseError::NoError) {
    if (out_error != nullptr) {
      *out_error = QString("JSON parse error at %1: %2")
                       .arg(perr.offset)
                       .arg(perr.errorString());
    }
    return false;
  }
  if (!doc.isObject()) {
    if (out_error != nullptr) {
      *out_error = "Map JSON root must be an object";
    }
    return false;
  }
  auto root = doc.object();

  outMap.name = root.value(NAME).toString("Unnamed Map");

  if (root.contains(COORD_SYSTEM)) {
    const QString coord_system =
        root.value(COORD_SYSTEM).toString().trimmed().toLower();
    if (coord_system == "world") {
      outMap.coordSystem = CoordSystem::World;
    } else {
      outMap.coordSystem = CoordSystem::Grid;
    }
  }

  constexpr int default_max_troops = 50;
  if (root.contains(MAX_TROOPS_PER_PLAYER)) {
    outMap.max_troops_per_player =
        root.value(MAX_TROOPS_PER_PLAYER).toInt(default_max_troops);
  }

  if (root.contains(GRID) && root.value(GRID).isObject()) {
    if (!readGrid(root.value(GRID).toObject(), outMap.grid)) {
      if (out_error != nullptr) {
        *out_error = "Invalid grid definition";
      }
      return false;
    }
  }

  if (root.contains(CAMERA) && root.value(CAMERA).isObject()) {
    readCamera(root.value(CAMERA).toObject(), outMap.camera);
  }

  if (root.contains(SPAWNS) && root.value(SPAWNS).isArray()) {
    readSpawns(root.value(SPAWNS).toArray(), outMap.spawns);
  }

  if (root.contains(FIRECAMPS) && root.value(FIRECAMPS).isArray()) {
    readFireCamps(root.value(FIRECAMPS).toArray(), outMap.firecamps);
  }

  if (root.contains(TERRAIN) && root.value(TERRAIN).isArray()) {
    readTerrain(root.value(TERRAIN).toArray(), outMap.terrain, outMap.grid,
                outMap.coordSystem);
  }

  if (root.contains(RIVERS) && root.value(RIVERS).isArray()) {
    readRivers(root.value(RIVERS).toArray(), outMap.rivers, outMap.grid,
               outMap.coordSystem);
  }

  if (root.contains(BRIDGES) && root.value(BRIDGES).isArray()) {
    readBridges(root.value(BRIDGES).toArray(), outMap.bridges, outMap.grid,
                outMap.coordSystem);
  }

  if (root.contains(BIOME) && root.value(BIOME).isObject()) {
    readBiome(root.value(BIOME).toObject(), outMap.biome);
  }

  if (root.contains(VICTORY) && root.value(VICTORY).isObject()) {
    readVictoryConfig(root.value(VICTORY).toObject(), outMap.victory);
  }

  return true;
}

} // namespace Game::Map
