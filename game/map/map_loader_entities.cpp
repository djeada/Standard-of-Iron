#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "forest_outline.h"
#include "json_keys.h"
#include "map/map_definition.h"
#include "map_loader_internal.h"
#include "systems/resource_json.h"
#include "units/spawn_type.h"
#include "wildlife/wildlife_placement.h"

namespace Game::Map::loader_detail {

using namespace JsonKeys;

namespace {

void read_wildlife_species(const QJsonObject& obj,
                           const GridDefinition& grid,
                           CoordSystem coord_sys,
                           Game::Wildlife::SpeciesConfig& out) {
  out.enabled = obj.value(WILDLIFE_ENABLED).toBool(out.enabled);
  out.group_count = obj.value(WILDLIFE_GROUPS).toInt(out.group_count);
  out.group_size_min = obj.value(WILDLIFE_GROUP_SIZE_MIN).toInt(out.group_size_min);
  out.group_size_max = obj.value(WILDLIFE_GROUP_SIZE_MAX).toInt(out.group_size_max);
  out.roam_radius = float(obj.value(WILDLIFE_ROAM_RADIUS).toDouble(out.roam_radius));
  out.move_speed = float(obj.value(WILDLIFE_MOVE_SPEED).toDouble(out.move_speed));
  out.flee_speed = float(obj.value(WILDLIFE_FLEE_SPEED).toDouble(out.flee_speed));
  out.alert_radius = float(obj.value(WILDLIFE_ALERT_RADIUS).toDouble(out.alert_radius));
  out.aggression = float(obj.value(WILDLIFE_AGGRESSION).toDouble(out.aggression));
  out.respawn = obj.value(WILDLIFE_RESPAWN).toBool(out.respawn);
  out.respawn_delay =
      float(obj.value(WILDLIFE_RESPAWN_DELAY).toDouble(out.respawn_delay));
  out.flight_height =
      float(obj.value(WILDLIFE_FLIGHT_HEIGHT).toDouble(out.flight_height));
  out.flyover_interval_min = float(
      obj.value(WILDLIFE_FLYOVER_INTERVAL_MIN).toDouble(out.flyover_interval_min));
  out.flyover_interval_max = float(
      obj.value(WILDLIFE_FLYOVER_INTERVAL_MAX).toDouble(out.flyover_interval_max));

  const float tile =
      coord_sys == CoordSystem::Grid ? std::max(0.0001F, grid.tile_size) : 1.0F;

  if (obj.value(WILDLIFE_WAVES).isArray()) {
    out.waves.clear();
    for (const auto value : obj.value(WILDLIFE_WAVES).toArray()) {
      if (!value.isObject()) {
        continue;
      }
      const QJsonObject wave_obj = value.toObject();
      Game::Wildlife::WildlifeWave wave;
      wave.timing = float(wave_obj.value(WILDLIFE_WAVE_TIMING).toDouble(0.0));
      wave.pack_size = wave_obj.value(WILDLIFE_WAVE_PACK_SIZE).toInt(wave.pack_size);
      wave.label = wave_obj.value(WILDLIFE_WAVE_LABEL).toString().toStdString();
      const QVector3D center = authored_position(float(wave_obj.value(X).toDouble(0.0)),
                                                 float(wave_obj.value(Z).toDouble(0.0)),
                                                 grid,
                                                 coord_sys);
      wave.area.x = center.x();
      wave.area.z = center.z();
      wave.area.radius =
          float(wave_obj.value(WILDLIFE_RADIUS).toDouble(double(out.roam_radius))) *
          tile;
      out.waves.push_back(wave);
    }
  }

  if (!obj.value(WILDLIFE_SPAWN_AREAS).isArray()) {
    return;
  }
  out.spawn_areas.clear();
  for (const auto value : obj.value(WILDLIFE_SPAWN_AREAS).toArray()) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject area_obj = value.toObject();
    const QVector3D center = authored_position(float(area_obj.value(X).toDouble(0.0)),
                                               float(area_obj.value(Z).toDouble(0.0)),
                                               grid,
                                               coord_sys);
    Game::Wildlife::SpawnArea area;
    area.x = center.x();
    area.z = center.z();
    area.radius =
        float(area_obj.value(WILDLIFE_RADIUS).toDouble(double(out.roam_radius))) * tile;
    out.spawn_areas.push_back(area);
  }
}

void read_wildlife_config(const QJsonObject& obj,
                          const GridDefinition& grid,
                          CoordSystem coord_sys,
                          Game::Wildlife::WildlifeSettings& out) {
  out = Game::Wildlife::default_settings();
  out.enabled = obj.value(WILDLIFE_ENABLED).toBool(true);
  out.seed =
      static_cast<std::uint32_t>(obj.value(WILDLIFE_SEED).toVariant().toULongLong());
  out.near_simulation_radius =
      float(obj.value(WILDLIFE_NEAR_RADIUS).toDouble(out.near_simulation_radius));
  out.far_simulation_radius =
      float(obj.value(WILDLIFE_FAR_RADIUS).toDouble(out.far_simulation_radius));

  if (obj.value(WILDLIFE_SHEEP).isObject()) {
    read_wildlife_species(
        obj.value(WILDLIFE_SHEEP).toObject(), grid, coord_sys, out.sheep);
  } else {
    out.sheep.enabled = false;
  }
  if (obj.value(WILDLIFE_WOLVES).isObject()) {
    read_wildlife_species(
        obj.value(WILDLIFE_WOLVES).toObject(), grid, coord_sys, out.wolves);
  } else {
    out.wolves.enabled = false;
  }
  if (obj.value(WILDLIFE_BIRDS).isObject()) {
    read_wildlife_species(
        obj.value(WILDLIFE_BIRDS).toObject(), grid, coord_sys, out.birds);
  } else {
    out.birds.enabled = false;
  }

  Game::Wildlife::sanitize(out);
}

void read_spawns(const QJsonArray& arr, std::vector<UnitSpawn>& out) {
  out.clear();
  out.reserve(arr.size());
  for (const auto spawn_val : arr) {
    auto spawn_obj = spawn_val.toObject();
    UnitSpawn spawn;
    const QString type_str = spawn_obj.value(TYPE).toString();
    if (!Game::Units::try_parse_spawn_type(type_str, spawn.type)) {
      qWarning() << "MapLoader: unknown spawn type" << type_str << "- skipping";
      continue;
    }
    spawn.id = spawn_obj.value(ID).toString();
    spawn.group = spawn_obj.value(GROUP).toString();
    spawn.x = float(spawn_obj.value(X).toDouble(0.0));
    spawn.z = float(spawn_obj.value(Z).toDouble(0.0));

    if (spawn_obj.contains(PLAYER_ID) && !spawn_obj.value(PLAYER_ID).isNull()) {
      spawn.player_id = spawn_obj.value(PLAYER_ID).toInt(0);
    } else {
      spawn.player_id = -1;
    }

    spawn.team_id = spawn_obj.value(TEAM_ID).toInt(0);
    constexpr int default_max_population = 60;
    spawn.max_population =
        spawn_obj.value(MAX_POPULATION).toInt(default_max_population);

    if (spawn_obj.contains(NATION)) {
      const QString nation_str = spawn_obj.value(NATION).toString();
      if (Game::Systems::NationID parsed_nation_id;
          Game::Systems::try_parse_nation_id(nation_str, parsed_nation_id)) {
        spawn.nation = parsed_nation_id;
      } else {
        qWarning() << "MapLoader: unknown nation" << nation_str << "- will use default";
      }
    }

    spawn.behavior = spawn_obj.value(QStringLiteral("behavior")).toString();
    spawn.guard_radius =
        float(spawn_obj.value(QStringLiteral("guard_radius")).toDouble(10.0));
    const QJsonArray patrol_waypoints =
        spawn_obj.value(QStringLiteral("patrol_waypoints")).toArray();
    spawn.patrol_waypoints.reserve(patrol_waypoints.size());
    for (const auto waypoint_val : patrol_waypoints) {
      const QJsonObject waypoint_obj = waypoint_val.toObject();
      spawn.patrol_waypoints.emplace_back(float(waypoint_obj.value(X).toDouble(0.0)),
                                          0.0F,
                                          float(waypoint_obj.value(Z).toDouble(0.0)));
    }

    out.push_back(spawn);
  }
}

void append_fire_camps_as_world_props(const QJsonArray& arr,
                                      std::vector<WorldProp>& out) {
  out.reserve(out.size() + arr.size());
  for (const auto camp_val : arr) {
    auto camp_obj = camp_val.toObject();
    WorldProp fire_camp;
    fire_camp.type = WorldProp::Type::FireCamp;
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

void append_world_props(const QJsonArray& arr, std::vector<WorldProp>& out) {
  out.reserve(out.size() + arr.size());
  for (const auto val : arr) {
    auto obj = val.toObject();
    const QString type_str = obj.value(JsonKeys::TYPE).toString();
    WorldProp prop;
    if (!world_prop_type_from_string(type_str, prop.type)) {
      qWarning() << "MapLoader: unknown world_prop type" << type_str << "- skipping";
      continue;
    }
    prop.x = float(obj.value(JsonKeys::X).toDouble(0.0));
    prop.z = float(obj.value(JsonKeys::Z).toDouble(0.0));
    prop.scale = float(obj.value(JsonKeys::SCALE).toDouble(1.0));
    prop.rotation = float(obj.value(JsonKeys::ROTATION).toDouble(0.0));
    prop.intensity = float(obj.value(JsonKeys::INTENSITY).toDouble(1.0));
    prop.radius = float(obj.value(JsonKeys::RADIUS).toDouble(3.0));
    prop.persistent = obj.value(JsonKeys::PERSISTENT).toBool(true);
    out.push_back(prop);
  }
}

void append_undead_wave_units_from_object(const QJsonObject& obj, UndeadWave& out) {
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    Game::Units::SpawnType spawn_type;
    if (!Game::Units::try_parse_spawn_type(it.key(), spawn_type)) {
      continue;
    }
    int const count = it.value().toInt(0);
    if (count <= 0) {
      continue;
    }
    out.units.push_back({spawn_type, count});
  }
}

void read_forests(const QJsonArray& arr, std::vector<Forest>& out) {
  out.clear();
  out.reserve(arr.size());
  int next_forest_index = 1;
  for (const auto val : arr) {
    auto obj = val.toObject();
    Forest forest;
    forest.id = obj.value(ID).toString().trimmed();
    if (forest.id.isEmpty()) {
      forest.id = QStringLiteral("forest_%1").arg(next_forest_index++);
    }
    forest.x = float(obj.value(X).toDouble(0.0));
    forest.z = float(obj.value(Z).toDouble(0.0));
    forest.radius = float(obj.value(RADIUS).toDouble(forest.radius));
    if (forest.radius <= 0.0F) {
      continue;
    }
    forest.outline_seed = forest_outline_seed(forest.x, forest.z);
    out.push_back(forest);
  }
}

void read_undead_zones(const QJsonArray& arr, std::vector<UndeadZone>& out) {
  out.clear();
  out.reserve(arr.size());
  int next_zone_index = 1;
  for (const auto val : arr) {
    auto obj = val.toObject();
    UndeadZone zone;
    zone.id = obj.value(ID).toString().trimmed();
    if (zone.id.isEmpty()) {
      zone.id = QStringLiteral("undead_zone_%1").arg(next_zone_index++);
    }

    if (!world_prop_type_from_string(obj.value(ANCHOR_TYPE).toString(),
                                     zone.anchor_type)) {
      zone.anchor_type = WorldProp::Type::Ruins;
    }

    zone.x = float(obj.value(X).toDouble(0.0));
    zone.z = float(obj.value(Z).toDouble(0.0));
    zone.radius = float(obj.value(RADIUS).toDouble(zone.radius));
    zone.leash_radius = float(
        obj.value(LEASH_RADIUS).toDouble(std::max(zone.radius, zone.leash_radius)));
    zone.owner_id = obj.value(OWNER_ID).toInt(zone.owner_id);
    zone.team_id = obj.value(TEAM_ID).toInt(zone.team_id);
    zone.fog_density = float(obj.value(FOG_DENSITY).toDouble(zone.fog_density));
    zone.wave_timeout_seconds =
        float(obj.value(WAVE_TIMEOUT).toDouble(zone.wave_timeout_seconds));
    zone.wave_delay_seconds =
        std::max(0.0F, float(obj.value(WAVE_DELAY).toDouble(zone.wave_delay_seconds)));

    if (obj.contains(AWAKEN_ON) && obj.value(AWAKEN_ON).isArray()) {
      const auto awaken_on = obj.value(AWAKEN_ON).toArray();
      for (const auto trigger_value : awaken_on) {
        QString const trigger = trigger_value.toString().trimmed().toLower();
        if (!trigger.isEmpty()) {
          zone.awaken_on.push_back(trigger);
        }
      }
    }

    if (zone.awaken_on.empty()) {
      zone.awaken_on.push_back(QStringLiteral("unit_enters_radius"));
    }

    if (obj.contains(CLEAR_REWARD) && obj.value(CLEAR_REWARD).isObject()) {
      Game::Systems::read_resource_overlay(obj.value(CLEAR_REWARD).toObject())
          .apply_to(zone.clear_reward);
    }

    if (obj.contains(WAVES) && obj.value(WAVES).isArray()) {
      const auto waves = obj.value(WAVES).toArray();
      for (const auto wave_value : waves) {
        auto wave_obj = wave_value.toObject();
        UndeadWave wave;
        wave.trigger =
            wave_obj.value(TRIGGER).toString(wave.trigger).trimmed().toLower();
        if (wave_obj.contains(UNITS) && wave_obj.value(UNITS).isObject()) {
          append_undead_wave_units_from_object(wave_obj.value(UNITS).toObject(), wave);
        }
        append_undead_wave_units_from_object(wave_obj, wave);
        if (!wave.units.empty()) {
          std::sort(wave.units.begin(),
                    wave.units.end(),
                    [](auto const& lhs, auto const& rhs) {
                      return static_cast<std::uint8_t>(lhs.type) <
                             static_cast<std::uint8_t>(rhs.type);
                    });
          zone.waves.push_back(std::move(wave));
        }
      }
    }

    if (zone.waves.empty()) {
      zone.waves = default_undead_waves();
    }

    out.push_back(std::move(zone));
  }
}

void append_undead_zone_fog(MapDefinition& out_map) {
  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;

  for (const auto& zone : out_map.undead_zones) {
    if (!(zone.fog_density > 0.0F)) {
      continue;
    }

    float world_x = zone.x;
    float world_z = zone.z;
    float world_radius = zone.radius;
    if (out_map.coordSystem == CoordSystem::Grid) {
      const float tile = std::max(min_tile_size, out_map.grid.tile_size);
      world_x =
          (zone.x - (out_map.grid.width * grid_center_offset - grid_center_offset)) *
          tile;
      world_z =
          (zone.z - (out_map.grid.height * grid_center_offset - grid_center_offset)) *
          tile;
      world_radius = zone.radius * tile;
    }

    out_map.fog_zones.push_back(
        undead_zone_fog(world_x, world_z, world_radius, zone.fog_density));
  }
}

auto read_structures(const QJsonArray& arr,
                     std::vector<StructureEntry>& out,
                     const GridDefinition& grid,
                     CoordSystem coord_sys,
                     QString* out_error) -> bool {
  out.clear();
  out.reserve(static_cast<std::size_t>(arr.size()));

  for (qsizetype index = 0; index < arr.size(); ++index) {
    const QJsonObject obj = arr[index].toObject();
    StructureEntry entry;
    const QString type_name = obj.value(TYPE).toString();
    if (!Game::Units::try_parse_spawn_type(type_name, entry.type) ||
        !Game::Units::is_building_spawn(entry.type)) {
      if (out_error != nullptr) {
        *out_error = QString("Invalid structure type '%1' at structures[%2]")
                         .arg(type_name)
                         .arg(index);
      }
      return false;
    }
    entry.id = obj.value("id").toString();
    entry.group = obj.value(GROUP).toString();
    entry.player_id = obj.value("player_id").toInt(0);
    entry.team_id = obj.value(TEAM_ID).toInt(0);
    entry.max_population = obj.value(MAX_POPULATION).toInt(60);
    entry.rotation = float(obj.value(ROTATION).toDouble(0.0));
    entry.nation = obj.value("nation").toString();

    if (entry.type == Game::Units::SpawnType::WallSegment) {
      const QJsonArray start = obj.value(START).toArray();
      const QJsonArray end = obj.value(END).toArray();
      if (start.size() < 2 || end.size() < 2) {
        if (out_error != nullptr) {
          *out_error =
              QString("Wall structure at structures[%1] requires start and end")
                  .arg(index);
        }
        return false;
      }
      entry.geometry = LineStructureGeometry{
          .start = authored_position(
              float(start[0].toDouble()), float(start[1].toDouble()), grid, coord_sys),
          .end = authored_position(
              float(end[0].toDouble()), float(end[1].toDouble()), grid, coord_sys),
          .width = float(obj.value(WIDTH).toDouble(2.0)),
      };
    } else {
      if (!obj.value(X).isDouble() || !obj.value(Z).isDouble()) {
        if (out_error != nullptr) {
          *out_error =
              QString("Point structure at structures[%1] requires x and z").arg(index);
        }
        return false;
      }
      entry.geometry =
          PointStructureGeometry{authored_position(float(obj.value(X).toDouble()),
                                                   float(obj.value(Z).toDouble()),
                                                   grid,
                                                   coord_sys)};
    }
    out.push_back(std::move(entry));
  }
  return true;
}

void read_fog_zones(const QJsonArray& arr,
                    std::vector<FogZone>& out,
                    const GridDefinition& grid,
                    CoordSystem coord_sys) {
  out.clear();
  out.reserve(arr.size());

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;

  for (const auto val : arr) {
    auto obj = val.toObject();
    FogZone zone;

    const float raw_x = static_cast<float>(obj.value(X).toDouble(0.0));
    const float raw_z = static_cast<float>(obj.value(Z).toDouble(0.0));
    const float raw_w = static_cast<float>(obj.value(WIDTH).toDouble(10.0));
    const float raw_h = static_cast<float>(obj.value(HEIGHT).toDouble(10.0));
    zone.density = static_cast<float>(obj.value(DENSITY).toDouble(0.6));

    if (coord_sys == CoordSystem::Grid) {
      const float tile = std::max(min_tile_size, grid.tile_size);
      zone.x = (raw_x - (grid.width * grid_center_offset - grid_center_offset)) * tile;
      zone.z = (raw_z - (grid.height * grid_center_offset - grid_center_offset)) * tile;
      zone.width = raw_w * tile;
      zone.height = raw_h * tile;
    } else {
      zone.x = raw_x;
      zone.z = raw_z;
      zone.width = raw_w;
      zone.height = raw_h;
    }

    out.push_back(zone);
  }
}

auto find_structure_in_spawns(const QJsonArray& authored_spawns,
                              QString* out_error) -> bool {
  for (qsizetype index = 0; index < authored_spawns.size(); ++index) {
    Game::Units::SpawnType type;
    const QString type_name = authored_spawns[index].toObject().value(TYPE).toString();
    if (Game::Units::try_parse_spawn_type(type_name, type) &&
        Game::Units::is_building_spawn(type)) {
      if (out_error != nullptr) {
        *out_error = QString("Structure '%1' found in spawns[%2]; use 'structures'")
                         .arg(type_name)
                         .arg(index);
      }
      return true;
    }
  }
  return false;
}

} // namespace

auto read_map_units(const QJsonObject& root,
                    MapDefinition& out_map,
                    QString* out_error) -> bool {
  if (root.contains(SPAWNS) && root.value(SPAWNS).isArray()) {
    if (find_structure_in_spawns(root.value(SPAWNS).toArray(), out_error)) {
      return false;
    }
    read_spawns(root.value(SPAWNS).toArray(), out_map.spawns);
  } else {
    out_map.spawns.clear();
  }

  if (root.contains(STRUCTURES)) {
    if (!root.value(STRUCTURES).isArray()) {
      if (out_error != nullptr) {
        *out_error = "Map 'structures' must be an array";
      }
      return false;
    }
    if (!read_structures(root.value(STRUCTURES).toArray(),
                         out_map.structures,
                         out_map.grid,
                         out_map.coordSystem,
                         out_error)) {
      return false;
    }
  } else {
    out_map.structures.clear();
  }
  return true;
}

void read_map_scenery(const QJsonObject& root, MapDefinition& out_map) {
  out_map.world_props.clear();
  if (root.contains(FIRECAMPS) && root.value(FIRECAMPS).isArray()) {
    append_fire_camps_as_world_props(root.value(FIRECAMPS).toArray(),
                                     out_map.world_props);
  }

  if (root.contains(WORLD_PROPS) && root.value(WORLD_PROPS).isArray()) {
    append_world_props(root.value(WORLD_PROPS).toArray(), out_map.world_props);
  }

  if (root.contains(UNDEAD_ZONES) && root.value(UNDEAD_ZONES).isArray()) {
    read_undead_zones(root.value(UNDEAD_ZONES).toArray(), out_map.undead_zones);
  } else {
    out_map.undead_zones.clear();
  }

  if (root.contains(FORESTS) && root.value(FORESTS).isArray()) {
    read_forests(root.value(FORESTS).toArray(), out_map.forests);
  } else {
    out_map.forests.clear();
  }
}

void read_map_fog(const QJsonObject& root, MapDefinition& out_map) {
  if (root.contains(FOG_ZONES) && root.value(FOG_ZONES).isArray()) {
    read_fog_zones(root.value(FOG_ZONES).toArray(),
                   out_map.fog_zones,
                   out_map.grid,
                   out_map.coordSystem);
  } else {
    out_map.fog_zones.clear();
  }

  append_undead_zone_fog(out_map);
}

auto read_map_wildlife(const QJsonObject& root, MapDefinition& out_map) -> bool {
  const bool wildlife_authored =
      root.contains(WILDLIFE) && root.value(WILDLIFE).isObject();
  if (wildlife_authored) {
    read_wildlife_config(root.value(WILDLIFE).toObject(),
                         out_map.grid,
                         out_map.coordSystem,
                         out_map.wildlife);
  }
  return wildlife_authored;
}

void finish_map_wildlife(MapDefinition& out_map, bool wildlife_authored) {
  if (!wildlife_authored) {
    out_map.wildlife = Game::Wildlife::default_settings_for_map(out_map);
  }
  Game::Wildlife::populate_missing_spawn_areas(out_map, out_map.wildlife);
}

} // namespace Game::Map::loader_detail
