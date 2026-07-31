#include "commander_preview.h"

#include <QJsonArray>

#include <algorithm>
#include <vector>

#include "game/map/spawn_cluster.h"
#include "game/systems/nation_id.h"
#include "game/units/troop_type.h"

namespace MapEditor {

namespace {

using Game::Map::WeightedSpawnPoint;

auto setup_positions(const QJsonArray& entries) -> std::vector<WeightedSpawnPoint> {
  std::vector<WeightedSpawnPoint> points;
  points.reserve(static_cast<std::size_t>(entries.size()));
  for (const QJsonValue& value : entries) {
    const QJsonObject entry = value.toObject();
    const QJsonObject position = entry.value(QStringLiteral("position")).toObject();
    if (position.isEmpty()) {
      continue;
    }
    points.push_back(
        {.x = static_cast<float>(position.value(QStringLiteral("x")).toDouble()),
         .z = static_cast<float>(position.value(QStringLiteral("z")).toDouble()),
         .weight = static_cast<float>(
             std::max(1, entry.value(QStringLiteral("count")).toInt(1)))});
  }
  return points;
}

auto map_troop_positions(const MapData& map,
                         int owner_id) -> std::vector<WeightedSpawnPoint> {
  std::vector<WeightedSpawnPoint> points;
  for (const TroopSpawnElement& spawn : map.troop_spawns()) {
    if (spawn.player_id == owner_id) {
      points.push_back({.x = spawn.x, .z = spawn.z});
    }
  }
  return points;
}

auto map_structure_positions(const MapData& map,
                             int owner_id) -> std::vector<WeightedSpawnPoint> {
  std::vector<WeightedSpawnPoint> points;
  for (const StructureElement& structure : map.structures()) {
    if (structure.player_id == owner_id) {
      points.push_back({.x = structure.x, .z = structure.z});
    }
  }
  return points;
}

auto resolve_position(const MapData& map,
                      const QJsonObject& setup,
                      int owner_id) -> QPointF {
  const std::vector<std::vector<WeightedSpawnPoint>> candidates{
      setup_positions(setup.value(QStringLiteral("starting_units")).toArray()),
      setup_positions(setup.value(QStringLiteral("starting_buildings")).toArray()),
      map_troop_positions(map, owner_id),
      map_structure_positions(map, owner_id)};

  for (const auto& points : candidates) {
    const auto center = Game::Map::densest_spawn_cluster(points);
    if (center.has_value()) {
      return {static_cast<double>(center->x), static_cast<double>(center->z)};
    }
  }

  return owner_id == k_local_owner_id ? QPointF(68.0, 70.0) : QPointF(132.0, 80.0);
}

auto append_commander(const MapData& map,
                      const QJsonObject& setup,
                      int owner_id,
                      const QString& label,
                      QVector<DerivedCommander>* out) -> void {
  if (setup.isEmpty()) {
    return;
  }

  DerivedCommander commander;
  commander.owner_id = owner_id;
  commander.label = label;
  commander.troop_type = resolve_commander_troop(
      setup.value(QStringLiteral("nation")).toString(),
      setup.value(QStringLiteral("commander_troop")).toString());

  const int authored_index = map.commander_spawn_index_for_player(owner_id);
  if (authored_index >= 0) {
    const TroopSpawnElement& spawn = map.troop_spawns()[authored_index];
    commander.authored_in_map = true;
    commander.troop_type = spawn.type;
    commander.position = QPointF(spawn.x, spawn.z);
  } else {
    commander.position = resolve_position(map, setup, owner_id);
  }

  out->append(commander);
}

} // namespace

auto resolve_commander_troop(const QString& nation,
                             const QString& configured) -> QString {
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  const bool parsed_nation = Game::Systems::try_parse_nation_id(nation, nation_id);

  const QString trimmed = configured.trimmed();
  if (!trimmed.isEmpty()) {
    Game::Units::TroopType troop_type{};
    if (Game::Units::try_parse_troop_type(trimmed, troop_type)) {
      const auto troop_nation = Game::Units::commander_troop_nation(troop_type);
      if (troop_nation.has_value() && (!parsed_nation || *troop_nation == nation_id)) {
        return Game::Units::troop_typeToQString(troop_type);
      }
    }
  }

  return Game::Units::troop_typeToQString(
      Game::Units::default_commander_troop_for_nation(
          parsed_nation ? nation_id : Game::Systems::NationID::RomanRepublic));
}

auto derive_mission_commanders(const MapData& map, const QJsonObject& mission_root)
    -> QVector<DerivedCommander> {
  QVector<DerivedCommander> commanders;

  const QJsonObject player_setup =
      mission_root.value(QStringLiteral("player_setup")).toObject();
  append_commander(
      map, player_setup, k_local_owner_id, QStringLiteral("player"), &commanders);

  const QJsonArray ai_setups =
      mission_root.value(QStringLiteral("ai_setups")).toArray();
  int owner_id = k_first_ai_owner_id;
  for (const QJsonValue& ai_value : ai_setups) {
    const QJsonObject ai_setup = ai_value.toObject();
    QString label = ai_setup.value(QStringLiteral("id")).toString();
    if (label.isEmpty()) {
      label = QStringLiteral("ai %1").arg(owner_id);
    }
    append_commander(map, ai_setup, owner_id, label, &commanders);
    ++owner_id;
  }

  return commanders;
}

} // namespace MapEditor
