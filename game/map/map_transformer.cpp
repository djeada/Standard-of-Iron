#include "map_transformer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QVector3D>
#include <qglobal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/nation_id.h"
#include "../systems/nation_registry.h"
#include "../systems/owner_registry.h"
#include "../systems/wall_network_service.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "base_options.h"
#include "core/entity.h"
#include "map/map_definition.h"
#include "terrain_service.h"
#include "units/unit.h"

namespace Game::Map {

namespace {
std::shared_ptr<Game::Units::UnitFactoryRegistry> s_registry;
std::unordered_map<int, int> s_player_team_overrides;
std::unordered_map<int, QString> s_base_assignments;
bool s_spectator_mode = false;

struct ResolvedBaseSeating {
  std::unordered_map<std::size_t, int> structure_owner;
  std::unordered_map<std::size_t, int> structure_population;
  std::unordered_map<int, QVector3D> player_offset;

  [[nodiscard]] auto owner_for(std::size_t index, int authored_player_id) const -> int {
    const auto it = structure_owner.find(index);
    return it == structure_owner.end() ? authored_player_id : it->second;
  }

  [[nodiscard]] auto population_for(std::size_t index,
                                    int authored_population) const -> int {
    const auto it = structure_population.find(index);
    return it == structure_population.end() ? authored_population : it->second;
  }

  [[nodiscard]] auto offset_for(int player_id) const -> QVector3D {
    const auto it = player_offset.find(player_id);
    return it == player_offset.end() ? QVector3D() : it->second;
  }
};

auto resolve_base_seating(const MapDefinition& def) -> ResolvedBaseSeating {
  ResolvedBaseSeating seating;
  if (s_base_assignments.empty()) {
    return seating;
  }

  const std::vector<BaseOption> options = collect_base_options(def);
  if (options.empty()) {
    return seating;
  }

  std::unordered_map<QString, const BaseOption*> by_key;
  by_key.reserve(options.size());
  for (const auto& option : options) {
    by_key.emplace(option.key, &option);
  }

  const std::unordered_map<int, QString> authored = default_base_assignments(def);
  std::unordered_map<QString, int> claimed_by;
  std::set<int> seated_players;

  for (const auto& [player_id, key] : s_base_assignments) {
    if (player_id <= 0) {
      continue;
    }
    const auto option_it = by_key.find(key);
    if (option_it == by_key.end()) {
      qWarning() << "MapTransformer: player" << player_id << "was seated at base" << key
                 << "which this map does not have; keeping the authored base";
      continue;
    }
    const auto claim = claimed_by.emplace(key, player_id);
    if (!claim.second) {
      qWarning() << "MapTransformer: base" << key << "was handed to both player"
                 << claim.first->second << "and player" << player_id
                 << "; keeping the first";
      continue;
    }

    const BaseOption& option = *option_it->second;
    seating.structure_owner[option.structure_index] = player_id;
    seated_players.insert(player_id);

    const auto authored_it = authored.find(player_id);
    if (authored_it == authored.end() || authored_it->second == key) {
      continue;
    }
    const auto authored_option_it = by_key.find(authored_it->second);
    if (authored_option_it == by_key.end()) {
      continue;
    }
    const BaseOption& home = *authored_option_it->second;
    seating.player_offset[player_id] = option.position - home.position;

    seating.structure_population[option.structure_index] = home.max_population;
  }

  for (const auto& option : options) {
    if (option.default_player_id <= 0) {
      continue;
    }
    if (seating.structure_owner.count(option.structure_index) != 0U) {
      continue;
    }

    if (seated_players.count(option.default_player_id) == 0U) {
      continue;
    }
    seating.structure_owner[option.structure_index] = Game::Core::NEUTRAL_OWNER_ID;
  }

  return seating;
}

constexpr float k_runtime_grid_center_offset = 0.5F;

enum class AuthoredUnitBehavior {
  Strategic,
  Guard,
  Hold,
  Patrol
};

auto runtime_grid_offset(int grid_size) -> float {
  return -(static_cast<float>(grid_size) * k_runtime_grid_center_offset -
           k_runtime_grid_center_offset);
}

auto runtime_world_to_grid(float world_coord, int grid_size) -> int {
  return static_cast<int>(std::round(world_coord - runtime_grid_offset(grid_size)));
}

auto runtime_grid_to_world(int grid_coord, int grid_size) -> float {
  return static_cast<float>(grid_coord) + runtime_grid_offset(grid_size);
}

auto effective_player_id_for_map_owner(int player_id) -> int {
  if (!s_player_team_overrides.empty() && player_id != Game::Core::NEUTRAL_OWNER_ID &&
      s_player_team_overrides.find(player_id) == s_player_team_overrides.end()) {
    return Game::Core::NEUTRAL_OWNER_ID;
  }
  return player_id;
}

auto resolve_nation_id_for_map_owner(int effective_player_id,
                                     const std::optional<Game::Systems::NationID>&
                                         explicit_nation) -> Game::Systems::NationID {
  if (explicit_nation.has_value()) {
    return *explicit_nation;
  }
  if (const auto* nation =
          Game::Systems::NationRegistry::instance().get_nation_for_player(
              effective_player_id)) {
    return nation->id;
  }
  return Game::Systems::NationRegistry::instance().default_nation_id();
}

auto resolve_nation_id_for_map_owner(int effective_player_id,
                                     const QString& authored_nation)
    -> Game::Systems::NationID {
  if (!authored_nation.trimmed().isEmpty()) {
    Game::Systems::NationID parsed_nation_id;
    if (Game::Systems::try_parse_nation_id(authored_nation, parsed_nation_id)) {
      return parsed_nation_id;
    }
    qWarning() << "MapTransformer: unknown nation" << authored_nation
               << "- using owner/default nation";
  }
  return resolve_nation_id_for_map_owner(effective_player_id,
                                         std::optional<Game::Systems::NationID>{});
}

auto parse_authored_unit_behavior(const QString& value) -> AuthoredUnitBehavior {
  const QString lowered = value.trimmed().toLower();
  if (lowered == "guard" || lowered == "defend" || lowered == "defensive") {
    return AuthoredUnitBehavior::Guard;
  }
  if (lowered == "hold" || lowered == "hold_position") {
    return AuthoredUnitBehavior::Hold;
  }
  if (lowered == "patrol") {
    return AuthoredUnitBehavior::Patrol;
  }
  return AuthoredUnitBehavior::Strategic;
}

auto is_scenario_controlled(AuthoredUnitBehavior behavior) -> bool {
  return behavior == AuthoredUnitBehavior::Guard ||
         behavior == AuthoredUnitBehavior::Hold ||
         behavior == AuthoredUnitBehavior::Patrol;
}

auto map_spawn_point_to_world(const QVector3D& point,
                              const MapDefinition& def) -> QVector3D {
  if (def.coordSystem != CoordSystem::Grid) {
    return point;
  }
  const float tile = std::max(0.0001F, def.grid.tile_size);
  return {(point.x() - (def.grid.width * 0.5F - 0.5F)) * tile,
          point.y(),
          (point.z() - (def.grid.height * 0.5F - 0.5F)) * tile};
}

void apply_authored_unit_behavior(Engine::Core::Entity& entity,
                                  AuthoredUnitBehavior behavior,
                                  const UnitSpawn& spawn,
                                  const QVector3D& world_pos,
                                  const MapDefinition& def) {
  if (behavior == AuthoredUnitBehavior::Guard) {
    auto* guard = entity.get_component<Engine::Core::GuardModeComponent>();
    if (guard == nullptr) {
      guard = entity.add_component<Engine::Core::GuardModeComponent>();
    }
    if (guard != nullptr) {
      guard->active = true;
      guard->guarded_entity_id = 0;
      guard->guard_position_x = world_pos.x();
      guard->guard_position_z = world_pos.z();
      guard->guard_radius = std::clamp(spawn.guard_radius, 2.0F, 60.0F);
      guard->returning_to_guard_position = false;
      guard->has_guard_target = true;
    }
  } else if (behavior == AuthoredUnitBehavior::Hold) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && Game::Units::can_use_hold_mode(unit->spawn_type)) {
      auto* hold = entity.get_component<Engine::Core::HoldModeComponent>();
      if (hold == nullptr) {
        hold = entity.add_component<Engine::Core::HoldModeComponent>();
      }
      if (hold != nullptr) {
        hold->active = true;
      }
    }
  } else if (behavior == AuthoredUnitBehavior::Patrol) {
    std::vector<std::pair<float, float>> waypoints;
    waypoints.reserve(spawn.patrol_waypoints.size() + 1U);
    waypoints.emplace_back(world_pos.x(), world_pos.z());
    for (const auto& authored_waypoint : spawn.patrol_waypoints) {
      const QVector3D waypoint = map_spawn_point_to_world(authored_waypoint, def);
      waypoints.emplace_back(waypoint.x(), waypoint.z());
    }

    if (waypoints.size() >= 2U) {
      auto* patrol = entity.get_component<Engine::Core::PatrolComponent>();
      if (patrol == nullptr) {
        patrol = entity.add_component<Engine::Core::PatrolComponent>();
      }
      if (patrol != nullptr) {
        patrol->waypoints = std::move(waypoints);
        patrol->current_waypoint = 1U;
        patrol->patrolling = true;
      }
    }
  }
}

auto spawn_map_unit(const Game::Units::SpawnParams& params,
                    Engine::Core::World& world,
                    std::vector<Engine::Core::EntityID>* runtime_unit_ids = nullptr)
    -> Engine::Core::Entity* {
  if (!s_registry) {
    qWarning() << "MapTransformer: no factory registry set; skipping spawn";
    return nullptr;
  }

  auto obj = s_registry->create(params.spawn_type, world, params);
  if (!obj) {
    qWarning() << "MapTransformer: no factory for spawn type"
               << Game::Units::spawn_typeToQString(params.spawn_type)
               << "- skipping spawn at" << params.position.x() << params.position.z();
    return nullptr;
  }

  if (runtime_unit_ids != nullptr) {
    runtime_unit_ids->push_back(obj->id());
  }

  Engine::Core::Entity* e = world.get_entity(obj->id());
  if (e == nullptr) {
    return nullptr;
  }

  return e;
}
} // namespace

void MapTransformer::setFactoryRegistry(
    std::shared_ptr<Game::Units::UnitFactoryRegistry> reg) {
  s_registry = std::move(reg);
}
auto MapTransformer::get_factory_registry()
    -> std::shared_ptr<Game::Units::UnitFactoryRegistry> {
  return s_registry;
}

void MapTransformer::set_local_owner_id(int owner_id) {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.set_local_player_id(owner_id);
}

auto MapTransformer::local_owner_id() -> int {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  return owners.get_local_player_id();
}

void MapTransformer::set_spectator_mode(bool enabled) {
  s_spectator_mode = enabled;
}

auto MapTransformer::spectator_mode() -> bool {
  return s_spectator_mode;
}

void MapTransformer::setPlayerTeamOverrides(
    const std::unordered_map<int, int>& overrides) {
  s_player_team_overrides = overrides;
}

void MapTransformer::clear_player_team_overrides() {
  s_player_team_overrides.clear();
}

void MapTransformer::set_base_assignments(
    const std::unordered_map<int, QString>& assignments) {
  s_base_assignments = assignments;
}

void MapTransformer::clear_base_assignments() {
  s_base_assignments.clear();
}

auto MapTransformer::apply_to_world(const MapDefinition& def,
                                    Engine::Core::World& world) -> MapRuntime {
  MapRuntime rt;
  rt.unit_ids.reserve(def.spawns.size());

  const ResolvedBaseSeating seating = resolve_base_seating(def);

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  std::set<int> unique_player_ids;
  std::unordered_map<int, int> player_id_to_team;

  for (const auto& spawn : def.spawns) {
    if (Game::Units::is_building_spawn(spawn.type)) {
      continue;
    }
    if (spawn.player_id == Game::Core::NEUTRAL_OWNER_ID) {
      continue;
    }
    unique_player_ids.insert(spawn.player_id);

    if (spawn.team_id > 0) {
      player_id_to_team[spawn.player_id] = spawn.team_id;
    }
  }

  for (const auto& structure : def.structures) {
    if (structure.player_id == Game::Core::NEUTRAL_OWNER_ID) {
      continue;
    }
    unique_player_ids.insert(structure.player_id);
    if (structure.team_id > 0) {
      player_id_to_team[structure.player_id] = structure.team_id;
    }
  }

  for (int const player_id : unique_player_ids) {
    bool const has_team_override =
        (s_player_team_overrides.find(player_id) != s_player_team_overrides.end());

    if (!s_player_team_overrides.empty() && !has_team_override) {
      continue;
    }

    if (owner_registry.get_owner_type(player_id) == Game::Systems::OwnerType::Neutral) {

      bool const is_local_player =
          !s_spectator_mode && (player_id == owner_registry.get_local_player_id());
      Game::Systems::OwnerType const owner_type = is_local_player
                                                      ? Game::Systems::OwnerType::Player
                                                      : Game::Systems::OwnerType::AI;

      std::string const owner_name =
          (is_local_player
               ? QCoreApplication::translate("MapTransformer", "Player %1")
               : QCoreApplication::translate("MapTransformer", "AI Player %1"))
              .arg(player_id)
              .toStdString();

      owner_registry.register_owner_with_id(player_id, owner_type, owner_name);
    }

    int final_team_id = 0;
    auto override_it = s_player_team_overrides.find(player_id);
    if (override_it != s_player_team_overrides.end()) {

      final_team_id = override_it->second;
    } else {

      auto team_it = player_id_to_team.find(player_id);
      if (team_it != player_id_to_team.end()) {
        final_team_id = team_it->second;
      }
    }
    owner_registry.set_owner_team(player_id, final_team_id);
  }

  for (const auto& s : def.spawns) {

    if (Game::Units::is_building_spawn(s.type)) {
      qWarning() << "MapTransformer: structure supplied through troop spawns; skipping"
                 << Game::Units::spawn_typeToQString(s.type);
      continue;
    }

    int const effective_player_id = effective_player_id_for_map_owner(s.player_id);

    float world_x = s.x;
    float world_z = s.z;
    if (def.coordSystem == CoordSystem::Grid) {
      const float tile = std::max(0.0001F, def.grid.tile_size);
      world_x = (s.x - (def.grid.width * 0.5F - 0.5F)) * tile;
      world_z = (s.z - (def.grid.height * 0.5F - 0.5F)) * tile;
    }

    const QVector3D camp_offset = seating.offset_for(s.player_id);
    world_x += camp_offset.x();
    world_z += camp_offset.z();

    auto& terrain = Game::Map::TerrainService::instance();
    if (terrain.is_initialized() && terrain.is_forbidden_world(world_x, world_z)) {
      const float tile = std::max(0.0001F, def.grid.tile_size);
      bool found = false;
      const int max_radius = 12;
      for (int r = 1; r <= max_radius && !found; ++r) {
        for (int ox = -r; ox <= r && !found; ++ox) {
          for (int oz = -r; oz <= r && !found; ++oz) {

            if (std::abs(ox) != r && std::abs(oz) != r) {
              continue;
            }
            float const cand_x = world_x + float(ox) * tile;
            float const cand_z = world_z + float(oz) * tile;
            if (!terrain.is_forbidden_world(cand_x, cand_z)) {
              world_x = cand_x;
              world_z = cand_z;
              found = true;
            }
          }
        }
      }
      if (!found) {
        qWarning() << "MapTransformer: spawn at" << s.x << s.z
                   << "is forbidden and no nearby free tile found; spawning anyway";
      }
    }

    Engine::Core::Entity* e = nullptr;
    const AuthoredUnitBehavior authored_behavior =
        parse_authored_unit_behavior(s.behavior);
    Game::Units::SpawnParams sp;
    sp.position = QVector3D(world_x, 0.0F, world_z);
    sp.player_id = effective_player_id;
    sp.spawn_type = s.type;
    sp.ai_controlled = !owner_registry.is_player(effective_player_id) &&
                       !is_scenario_controlled(authored_behavior);
    sp.max_population = s.max_population;
    sp.nation_id = resolve_nation_id_for_map_owner(effective_player_id, s.nation);

    e = spawn_map_unit(sp, world, &rt.unit_ids);
    if (e == nullptr) {
      continue;
    }

    apply_authored_unit_behavior(*e, authored_behavior, s, sp.position, def);

    if (auto* t = e->get_component<Engine::Core::TransformComponent>()) {
      qInfo() << "Spawned" << Game::Units::spawn_typeToQString(s.type)
              << "id=" << e->get_id() << "at"
              << QVector3D(t->position.x, t->position.y, t->position.z)
              << "(coordSystem="
              << (def.coordSystem == CoordSystem::Grid ? "Grid" : "World") << ")";
    }
  }

  for (std::size_t structure_index = 0; structure_index < def.structures.size();
       ++structure_index) {
    const auto& structure = def.structures[structure_index];
    const int seated_player_id =
        seating.owner_for(structure_index, structure.player_id);

    Game::Units::SpawnParams sp;
    sp.player_id = effective_player_id_for_map_owner(seated_player_id);
    sp.spawn_type = structure.type;
    sp.ai_controlled = !owner_registry.is_player(sp.player_id);
    sp.max_population =
        seating.population_for(structure_index, structure.max_population);
    sp.nation_id = resolve_nation_id_for_map_owner(sp.player_id, structure.nation);

    const QVector3D structure_offset =
        seating.structure_owner.count(structure_index) != 0U
            ? QVector3D()
            : seating.offset_for(structure.player_id);

    if (const auto* point = std::get_if<PointStructureGeometry>(&structure.geometry)) {
      sp.position = point->position + structure_offset;

      sp.rotation_y = structure.rotation;
      spawn_map_unit(sp, world);
      continue;
    }

    const auto* line = std::get_if<LineStructureGeometry>(&structure.geometry);
    if (line == nullptr || structure.type != Game::Units::SpawnType::WallSegment) {
      qWarning() << "MapTransformer: unsupported line geometry for structure"
                 << Game::Units::spawn_typeToQString(structure.type);
      continue;
    }

    const QVector3D line_start = line->start + structure_offset;
    const QVector3D line_end = line->end + structure_offset;
    const auto snapped_start = Game::Systems::WallGridPosition{
        .x = Game::Systems::WallNetworkService::snap_grid_coordinate(
            runtime_world_to_grid(line_start.x(), def.grid.width)),
        .z = Game::Systems::WallNetworkService::snap_grid_coordinate(
            runtime_world_to_grid(line_start.z(), def.grid.height))};
    const auto snapped_end = Game::Systems::WallGridPosition{
        .x = Game::Systems::WallNetworkService::snap_grid_coordinate(
            runtime_world_to_grid(line_end.x(), def.grid.width)),
        .z = Game::Systems::WallNetworkService::snap_grid_coordinate(
            runtime_world_to_grid(line_end.z(), def.grid.height))};

    for (const auto& segment :
         Game::Systems::WallNetworkService::build_axis_aligned_chain(snapped_start,
                                                                     snapped_end)) {
      sp.position = QVector3D(runtime_grid_to_world(segment.x, def.grid.width),
                              0.0F,
                              runtime_grid_to_world(segment.z, def.grid.height));
      spawn_map_unit(sp, world);
    }
  }

  if (s_registry) {

    Game::Systems::BuildingCollisionRegistry::instance().clear_authored_obstacles();
  }

  return rt;
}

} // namespace Game::Map
