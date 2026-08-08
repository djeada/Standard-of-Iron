#include "mission_setup_coordinator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSet>
#include <QVariantMap>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "campaign_manager.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/mission_context.h"
#include "game/map/wave_archetype_catalog.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/visuals/team_colors.h"
#include "game_state_restorer.h"
#include "mission_commander_setup.h"
#include "utils/resource_utils.h"

namespace App::Core {

namespace {

auto classify_wave_direction(const QVector3D& entry_point) -> QString {
  const float x = entry_point.x();
  const float z = entry_point.z();
  const float ax = std::abs(x);
  const float az = std::abs(z);

  constexpr float k_direction_bias = 1.25F;
  if (ax > az * k_direction_bias) {
    return x >= 0.0F ? QCoreApplication::translate("MissionSetupCoordinator", "east")
                     : QCoreApplication::translate("MissionSetupCoordinator", "west");
  }
  if (az > ax * k_direction_bias) {
    return z >= 0.0F ? QCoreApplication::translate("MissionSetupCoordinator", "south")
                     : QCoreApplication::translate("MissionSetupCoordinator", "north");
  }
  if (x >= 0.0F && z >= 0.0F) {
    return QCoreApplication::translate("MissionSetupCoordinator", "southeast");
  }
  if (x >= 0.0F && z < 0.0F) {
    return QCoreApplication::translate("MissionSetupCoordinator", "northeast");
  }
  if (x < 0.0F && z >= 0.0F) {
    return QCoreApplication::translate("MissionSetupCoordinator", "southwest");
  }
  return QCoreApplication::translate("MissionSetupCoordinator", "northwest");
}

constexpr float k_skirmish_aggression = 0.6F;
constexpr float k_skirmish_defense = 0.55F;
constexpr float k_skirmish_harassment = 0.5F;

auto is_scenario_controlled_behavior(Game::Mission::UnitBehavior behavior) -> bool {
  return behavior == Game::Mission::UnitBehavior::Guard ||
         behavior == Game::Mission::UnitBehavior::Hold ||
         behavior == Game::Mission::UnitBehavior::Patrol;
}

void assign_wave_phases(std::vector<PendingMissionWave>& waves, std::size_t begin) {
  std::vector<int> authored;
  std::vector<float> derived_times;
  for (std::size_t i = begin; i < waves.size(); ++i) {
    const auto& wave = waves[i];
    if (wave.authored_phase.has_value()) {
      const int phase = std::max(1, *wave.authored_phase);
      if (std::find(authored.begin(), authored.end(), phase) == authored.end()) {
        authored.push_back(phase);
      }
      continue;
    }
    const float trigger_time = wave.trigger_time;
    const auto duplicate = std::find_if(
        derived_times.begin(), derived_times.end(), [trigger_time](float existing) {
          return std::abs(existing - trigger_time) < 0.01F;
        });
    if (duplicate == derived_times.end()) {
      derived_times.push_back(trigger_time);
    }
  }
  std::sort(authored.begin(), authored.end());
  std::sort(derived_times.begin(), derived_times.end());

  const int phase_count =
      std::max(1, static_cast<int>(authored.size() + derived_times.size()));
  const auto derived_offset = static_cast<int>(authored.size());

  for (std::size_t i = begin; i < waves.size(); ++i) {
    auto& wave = waves[i];
    if (wave.authored_phase.has_value()) {
      const int phase = std::max(1, *wave.authored_phase);
      const auto slot = std::find(authored.begin(), authored.end(), phase);
      wave.phase_index = static_cast<int>(std::distance(authored.begin(), slot)) + 1;
    } else {
      const auto slot = std::find_if(
          derived_times.begin(), derived_times.end(), [&wave](float trigger_time) {
            return std::abs(trigger_time - wave.trigger_time) < 0.01F;
          });
      wave.phase_index =
          slot == derived_times.end()
              ? phase_count
              : derived_offset +
                    static_cast<int>(std::distance(derived_times.begin(), slot)) + 1;
    }
    wave.phase_count = phase_count;
  }

  for (std::size_t i = begin; i < waves.size(); ++i) {
    waves[i].final_wave = waves[i].phase_index == phase_count;
  }
}

} // namespace

auto resolve_defense_reference(Engine::Core::World& world,
                               int local_owner_id) -> QVector3D {

  QVector3D barracks;
  QVector3D halls;
  QVector3D troops;
  int barracks_count = 0;
  int hall_count = 0;
  int troop_count = 0;

  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->owner_id != local_owner_id ||
        unit->health <= 0) {
      continue;
    }

    const QVector3D position(
        transform->position.x, transform->position.y, transform->position.z);
    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      barracks += position;
      barracks_count++;
    } else if (entity->has_component<Engine::Core::BuildingComponent>()) {
      if (unit->spawn_type != Game::Units::SpawnType::WallSegment &&
          unit->spawn_type != Game::Units::SpawnType::WallGate) {
        halls += position;
        hall_count++;
      }
    } else {
      troops += position;
      troop_count++;
    }
  }

  if (barracks_count > 0) {
    return barracks / static_cast<float>(barracks_count);
  }
  if (hall_count > 0) {
    return halls / static_cast<float>(hall_count);
  }
  if (troop_count > 0) {
    return troops / static_cast<float>(troop_count);
  }
  return {0.0F, 0.0F, 0.0F};
}

auto wave_unit_total(const PendingMissionWave& wave) -> int {
  int total = 0;
  for (const auto& comp : wave.composition) {
    total += std::max(1, comp.count);
  }
  return total;
}

namespace {

auto wave_display_name(const PendingMissionWave& wave) -> QString {
  if (!wave.label.isEmpty()) {
    return wave.label;
  }
  QString name = wave.ai_id;
  name.replace('_', ' ');
  if (name.isEmpty()) {
    return QCoreApplication::translate("MissionSetupCoordinator", "Enemy");
  }
  return name;
}

auto wave_direction(const PendingMissionWave& wave) -> QString {
  return classify_wave_direction(wave.entry_world_position -
                                 wave.defense_reference_world_position);
}

void order_wave_advance(Engine::Core::World& world,
                        const PendingMissionWave& wave,
                        const std::vector<Engine::Core::EntityID>& units) {
  if (units.empty()) {
    return;
  }

  constexpr float k_approach_fraction = 0.85F;
  constexpr float k_min_advance_distance = 4.0F;

  const QVector3D from = wave.entry_world_position;
  const QVector3D to = wave.defense_reference_world_position;
  const QVector3D offset = to - from;
  if (offset.length() < k_min_advance_distance) {
    return;
  }

  const QVector3D target = Game::Systems::CommandService::snap_to_walkable_ground(
      from + (offset * k_approach_fraction));

  const auto plan =
      Game::Systems::CommandService::plan_ground_move(world, units, target);
  if (plan.positions.size() != units.size()) {
    return;
  }

  Game::Systems::CommandService::MoveOptions options;
  options.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  Game::Systems::CommandService::move_units(world, units, plan.positions, options);
}

} // namespace

auto wave_incoming_announcement(const PendingMissionWave& wave) -> QString {
  return QCoreApplication::translate(
             "MissionSetupCoordinator",
             "Wave %1/%2 forming to the %3: %4, roughly %5 troops. Brace.")
      .arg(wave.phase_index)
      .arg(wave.phase_count)
      .arg(wave_direction(wave), wave_display_name(wave))
      .arg(wave_unit_total(wave));
}

auto wave_cleared_announcement(const PendingMissionWave& wave) -> QString {
  return QCoreApplication::translate("MissionSetupCoordinator",
                                     "Wave %1/%2 broken. The line holds.")
      .arg(wave.phase_index)
      .arg(wave.phase_count);
}

auto all_waves_cleared_announcement(const PendingMissionWave& wave) -> QString {
  return QCoreApplication::translate(
             "MissionSetupCoordinator",
             "All %1 assault phases broken. Nothing else is coming.")
      .arg(wave.phase_count);
}

auto MissionSetupCoordinator::apply_mission_setup(
    const MissionSetupApplyContext& ctx) const -> MissionSetupEffects {
  MissionSetupEffects effects;
  if (!ctx.campaign.current_mission_context().is_campaign()) {
    return effects;
  }

  if (!ctx.campaign.current_mission_definition().has_value()) {
    return effects;
  }

  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    qWarning() << "Mission setup skipped: unit factory registry missing";
    return effects;
  }

  const auto& mission = *ctx.campaign.current_mission_definition();
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  auto& nation_registry = Game::Systems::NationRegistry::instance();

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path =
      Utils::Resources::resolve_resource_path(ctx.level.map_path);
  bool map_loaded =
      Game::Map::MapLoader::load_from_json_file(resolved_map_path, map_def, &map_error);
  if (!map_loaded) {
    qWarning() << "Mission setup: failed to load map definition for"
               << ctx.level.map_path << "resolved to" << resolved_map_path << "-"
               << map_error;
  }

  bool has_map_spawns = true;
  if (map_loaded) {
    has_map_spawns = !map_def.spawns.empty();
  }

  bool has_mission_spawns = !mission.player_setup.starting_units.empty() ||
                            !mission.player_setup.starting_buildings.empty();
  for (const auto& ai_setup : mission.ai_setups) {
    if (!ai_setup.starting_units.empty() || !ai_setup.starting_buildings.empty()) {
      has_mission_spawns = true;
      break;
    }
  }

  if (has_mission_spawns && !has_map_spawns) {
    std::vector<Engine::Core::EntityID> to_remove;
    auto entities = ctx.world.get_entities_with<Engine::Core::UnitComponent>();
    to_remove.reserve(entities.size());
    for (auto* entity : entities) {
      if (entity != nullptr) {
        to_remove.push_back(entity->get_id());
      }
    }
    for (const auto id : to_remove) {
      ctx.world.destroy_entity(id);
    }
  }

  auto resolve_nation_id = [&nation_registry](const QString& nation_str) {
    const auto parsed = Game::Systems::nation_id_from_string(nation_str.toStdString());
    if (parsed.has_value()) {
      return parsed.value();
    }
    return nation_registry.default_nation_id();
  };

  auto mission_position_to_world = [&](const Game::Mission::Position& pos) {
    float world_x = pos.x;
    float world_z = pos.z;
    if (map_loaded && map_def.coordSystem == Game::Map::CoordSystem::Grid) {
      const float tile = std::max(0.0001F, map_def.grid.tile_size);
      world_x = (pos.x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
    } else if (!map_loaded) {
      const float tile = std::max(0.0001F, ctx.level.tile_size);
      world_x = (pos.x - (ctx.level.grid_width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (ctx.level.grid_height * 0.5F - 0.5F)) * tile;
    }
    return QVector3D(world_x, 0.0F, world_z);
  };

  auto apply_team_color = [&](Engine::Core::Entity* entity, int owner_id) {
    if (entity == nullptr) {
      return;
    }
    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (renderable == nullptr) {
      return;
    }
    const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();
  };

  auto parse_color = [](const QString& color_name, std::array<float, 3>& out) -> bool {
    if (color_name.isEmpty()) {
      return false;
    }

    const QString trimmed = color_name.trimmed();
    if (trimmed.startsWith('#') && trimmed.length() == 7) {
      bool ok = false;
      int const r = trimmed.mid(1, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      int const g = trimmed.mid(3, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      int const b = trimmed.mid(5, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      constexpr float scale = 255.0F;
      out = {r / scale, g / scale, b / scale};
      return true;
    }

    const QString lowered = trimmed.toLower();
    if (lowered == "red") {
      out = {1.00F, 0.30F, 0.30F};
      return true;
    }
    if (lowered == "brown") {
      out = {0.55F, 0.36F, 0.18F};
      return true;
    }
    if (lowered == "blue") {
      out = {0.20F, 0.55F, 1.00F};
      return true;
    }
    if (lowered == "green") {
      out = {0.20F, 0.80F, 0.40F};
      return true;
    }
    if (lowered == "yellow") {
      out = {1.00F, 0.80F, 0.20F};
      return true;
    }
    if (lowered == "orange") {
      out = {0.95F, 0.55F, 0.10F};
      return true;
    }
    if (lowered == "white") {
      out = {0.95F, 0.95F, 0.95F};
      return true;
    }
    if (lowered == "black") {
      out = {0.15F, 0.15F, 0.15F};
      return true;
    }

    return false;
  };

  auto apply_owner_color = [&](int owner_id, const QString& color_name) {
    std::array<float, 3> color{};
    if (!parse_color(color_name, color)) {
      return;
    }
    owner_registry.set_owner_color(owner_id, color[0], color[1], color[2]);
  };

  auto spawn_units_for_owner = [&](int owner_id,
                                   const Game::Systems::NationID nation_id,
                                   const std::vector<Game::Mission::UnitSetup>& units) {
    const bool ai_controlled = owner_registry.is_ai(owner_id);
    for (const auto& unit_setup : units) {
      const auto spawn_type =
          Game::Units::spawn_typeFromString(unit_setup.type.toStdString());
      if (!spawn_type.has_value()) {
        qWarning() << "Mission setup: unknown unit type" << unit_setup.type;
        continue;
      }

      const int count = std::max(1, unit_setup.count);
      const int grid =
          static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
      const QVector3D base_pos = mission_position_to_world(unit_setup.position);
      float base_tile_size = ctx.level.tile_size;
      if (map_loaded && map_def.coordSystem == Game::Map::CoordSystem::Grid) {
        base_tile_size = map_def.grid.tile_size;
      }
      const float spacing = std::max(0.5F, base_tile_size * 1.2F);

      for (int i = 0; i < count; ++i) {
        const int row = i / grid;
        const int col = i % grid;
        const float offset_x = (float(col) - (grid - 1) * 0.5F) * spacing;
        const float offset_z = (float(row) - (grid - 1) * 0.5F) * spacing;
        const QVector3D pos =
            QVector3D(base_pos.x() + offset_x, base_pos.y(), base_pos.z() + offset_z);

        Game::Units::SpawnParams sp;
        sp.position = pos;
        sp.player_id = owner_id;
        sp.spawn_type = spawn_type.value();
        sp.ai_controlled =
            ai_controlled && !is_scenario_controlled_behavior(unit_setup.behavior);
        sp.nation_id = nation_id;

        auto unit = reg->create(sp.spawn_type, ctx.world, sp);
        if (!unit) {
          qWarning() << "Mission setup: failed to spawn unit" << unit_setup.type
                     << "for owner" << owner_id;
          continue;
        }
        auto* entity = ctx.world.get_entity(unit->id());
        apply_team_color(entity, owner_id);

        if (entity == nullptr) {
          continue;
        }

        if (unit_setup.behavior == Game::Mission::UnitBehavior::Guard) {
          auto* guard = entity->get_component<Engine::Core::GuardModeComponent>();
          if (guard == nullptr) {
            guard = entity->add_component<Engine::Core::GuardModeComponent>();
          }
          if (guard != nullptr) {
            guard->active = true;
            guard->guarded_entity_id = 0;
            guard->guard_position_x = pos.x();
            guard->guard_position_z = pos.z();
            guard->guard_radius = std::clamp(unit_setup.guard_radius, 2.0F, 60.0F);
            guard->returning_to_guard_position = false;
            guard->has_guard_target = true;
          }
        } else if (unit_setup.behavior == Game::Mission::UnitBehavior::Hold) {
          const auto* hold_unit = entity->get_component<Engine::Core::UnitComponent>();
          if (hold_unit != nullptr &&
              Game::Units::can_use_hold_mode(hold_unit->spawn_type)) {
            auto* hold = entity->get_component<Engine::Core::HoldModeComponent>();
            if (hold == nullptr) {
              hold = entity->add_component<Engine::Core::HoldModeComponent>();
            }
            if (hold != nullptr) {
              hold->active = true;
            }
          }
        } else if (unit_setup.behavior == Game::Mission::UnitBehavior::Patrol) {
          std::vector<std::pair<float, float>> waypoints;
          waypoints.reserve(unit_setup.patrol_waypoints.size() + 1U);
          waypoints.emplace_back(pos.x(), pos.z());
          for (const auto& authored_waypoint : unit_setup.patrol_waypoints) {
            const QVector3D waypoint = mission_position_to_world(authored_waypoint);
            waypoints.emplace_back(waypoint.x(), waypoint.z());
          }

          if (waypoints.size() >= 2U) {
            auto* patrol = entity->get_component<Engine::Core::PatrolComponent>();
            if (patrol == nullptr) {
              patrol = entity->add_component<Engine::Core::PatrolComponent>();
            }
            if (patrol != nullptr) {
              patrol->waypoints = std::move(waypoints);
              patrol->current_waypoint = 1U;
              patrol->patrolling = true;
            }
          } else {
            auto* guard = entity->get_component<Engine::Core::GuardModeComponent>();
            if (guard == nullptr) {
              guard = entity->add_component<Engine::Core::GuardModeComponent>();
            }
            if (guard != nullptr) {
              guard->active = true;
              guard->guard_position_x = pos.x();
              guard->guard_position_z = pos.z();
              guard->guard_radius = std::clamp(unit_setup.guard_radius, 2.0F, 60.0F);
              guard->has_guard_target = true;
            }
          }
        }
      }
    }
  };

  auto spawn_commander_for_owner =
      [&](int owner_id,
          const Game::Systems::NationID nation_id,
          const QString& commander_troop,
          const App::Core::ResolvedCommanderPosition& position) {
        if (commander_troop.trimmed().isEmpty()) {
          return;
        }
        const auto spawn_type =
            Game::Units::spawn_typeFromString(commander_troop.trimmed().toStdString());
        if (!spawn_type.has_value()) {
          qWarning() << "Mission setup: unknown commander troop" << commander_troop;
          return;
        }
        const auto troop_type = Game::Units::spawn_typeToTroopType(*spawn_type);
        if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
          qWarning() << "Mission setup: non-commander troop configured as commander"
                     << commander_troop;
          return;
        }
        for (auto* entity :
             ctx.world.get_entities_with<Engine::Core::CommanderComponent>()) {
          if (entity == nullptr) {
            continue;
          }
          const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
          if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
            return;
          }
        }
        Game::Units::SpawnParams sp;
        if (position.space == App::Core::CommanderPositionSpace::World) {
          sp.position = QVector3D(position.position.x, 0.0F, position.position.z);
        } else {
          sp.position = mission_position_to_world(position.position);
        }
        sp.player_id = owner_id;
        sp.spawn_type = *spawn_type;
        sp.ai_controlled = owner_registry.is_ai(owner_id);
        sp.nation_id = nation_id;
        auto unit = reg->create(sp.spawn_type, ctx.world, sp);
        if (!unit) {
          qWarning() << "Mission setup: failed to spawn commander" << commander_troop
                     << "for owner" << owner_id;
          return;
        }
        apply_team_color(ctx.world.get_entity(unit->id()), owner_id);
      };

  auto existing_owner_spawn_anchors = [&](int owner_id) {
    std::vector<App::Core::ExistingOwnerSpawnAnchor> anchors;
    auto entities = ctx.world.get_entities_with<Engine::Core::UnitComponent>();
    anchors.reserve(entities.size());
    for (auto* entity : entities) {
      if (entity == nullptr) {
        continue;
      }

      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->owner_id != owner_id ||
          unit->health <= 0) {
        continue;
      }

      App::Core::ExistingOwnerSpawnAnchor anchor;
      anchor.position = {transform->position.x, transform->position.z};
      anchor.is_building = Game::Units::is_building_spawn(unit->spawn_type);
      anchors.push_back(anchor);
    }
    return anchors;
  };

  auto spawn_buildings_for_owner =
      [&](int owner_id,
          const Game::Systems::NationID nation_id,
          const std::vector<Game::Mission::BuildingSetup>& buildings) {
        const bool ai_controlled = owner_registry.is_ai(owner_id);
        for (const auto& building_setup : buildings) {
          const auto spawn_type =
              Game::Units::spawn_typeFromString(building_setup.type.toStdString());
          if (!spawn_type.has_value()) {
            qWarning() << "Mission setup: unknown building type" << building_setup.type;
            continue;
          }

          const QVector3D pos = mission_position_to_world(building_setup.position);

          Game::Units::SpawnParams sp;
          sp.position = pos;
          sp.player_id = owner_id;
          sp.spawn_type = spawn_type.value();
          sp.ai_controlled = ai_controlled;
          sp.nation_id = nation_id;
          sp.max_population = building_setup.max_population;

          auto unit = reg->create(sp.spawn_type, ctx.world, sp);
          if (!unit) {
            qWarning() << "Mission setup: failed to spawn building"
                       << building_setup.type << "for owner" << owner_id;
            continue;
          }
          apply_team_color(ctx.world.get_entity(unit->id()), owner_id);
        }
      };

  const int local_owner_id = ctx.local_owner_id;
  if (owner_registry.get_owner_type(local_owner_id) ==
      Game::Systems::OwnerType::Neutral) {
    owner_registry.register_owner_with_id(
        local_owner_id,
        Game::Systems::OwnerType::Player,
        QCoreApplication::translate("MissionSetupCoordinator", "Player %1")
            .arg(local_owner_id)
            .toStdString());
  }
  owner_registry.set_owner_team(local_owner_id, 0);

  const auto player_nation_id = resolve_nation_id(mission.player_setup.nation);
  nation_registry.set_player_nation(local_owner_id, player_nation_id);
  apply_owner_color(local_owner_id, mission.player_setup.color);

  const QString player_commander_troop = App::Core::resolve_commander_troop(
      mission.player_setup.nation, mission.player_setup.commander_troop);
  const auto player_commander_pos = App::Core::resolve_commander_position(
      mission.player_setup.starting_units,
      mission.player_setup.starting_buildings,
      existing_owner_spawn_anchors(local_owner_id),
      {68.0F, 70.0F});
  spawn_commander_for_owner(
      local_owner_id, player_nation_id, player_commander_troop, player_commander_pos);
  spawn_units_for_owner(
      local_owner_id, player_nation_id, mission.player_setup.starting_units);
  spawn_buildings_for_owner(
      local_owner_id, player_nation_id, mission.player_setup.starting_buildings);

  const QVector3D defense_reference_world_position =
      resolve_defense_reference(ctx.world, local_owner_id);

  int ai_owner_id = 2;
  int default_team_id = 1;
  for (const auto& ai_setup : mission.ai_setups) {
    if (owner_registry.get_owner_type(ai_owner_id) ==
        Game::Systems::OwnerType::Neutral) {
      owner_registry.register_owner_with_id(
          ai_owner_id,
          Game::Systems::OwnerType::AI,
          QCoreApplication::translate("MissionSetupCoordinator", "AI Player %1")
              .arg(ai_owner_id)
              .toStdString());
    }

    int team_id = 0;
    if (ai_setup.team_id.has_value()) {
      team_id = ai_setup.team_id.value();
    } else {
      team_id = default_team_id++;
    }

    owner_registry.set_owner_team(ai_owner_id, team_id);

    const auto ai_nation_id = resolve_nation_id(ai_setup.nation);
    nation_registry.set_player_nation(ai_owner_id, ai_nation_id);
    apply_owner_color(ai_owner_id, ai_setup.color);

    const QString ai_commander_troop =
        App::Core::resolve_commander_troop(ai_setup.nation, ai_setup.commander_troop);
    const auto ai_commander_pos =
        App::Core::resolve_commander_position(ai_setup.starting_units,
                                              ai_setup.starting_buildings,
                                              existing_owner_spawn_anchors(ai_owner_id),
                                              {132.0F, 80.0F});
    spawn_commander_for_owner(
        ai_owner_id, ai_nation_id, ai_commander_troop, ai_commander_pos);
    spawn_units_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_units);
    spawn_buildings_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_buildings);

    ai_owner_id++;
  }

  {
    auto built = build_pending_mission_waves(
        {.mission = mission,
         .mission_difficulty = ctx.campaign.current_mission_context().difficulty,
         .level = ctx.level,
         .defense_reference_world_position = defense_reference_world_position});
    ctx.pending_waves.insert(ctx.pending_waves.end(),
                             std::make_move_iterator(built.begin()),
                             std::make_move_iterator(built.end()));
  }

  auto entities = ctx.world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : entities) {
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    apply_team_color(entity, unit->owner_id);
  }

  if (auto* ai_system = ctx.world.get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();

    int ai_id = 2;
    for (const auto& ai_setup : mission.ai_setups) {
      Game::Systems::AI::AIStrategy strategy = Game::Systems::AI::AIStrategy::Defensive;

      if (ai_setup.strategy.has_value()) {
        strategy = Game::Systems::AI::AIStrategyFactory::parse_strategy(
            ai_setup.strategy.value());
      }

      ai_system->set_ai_strategy(ai_id,
                                 strategy,
                                 ai_setup.personality.aggression,
                                 ai_setup.personality.defense,
                                 ai_setup.personality.harassment,
                                 ai_setup.difficulty);
      ai_id++;
    }
  }

  int const prev_selected_player = ctx.selected_player_id;
  GameStateRestorer::rebuild_registries_after_load(
      &ctx.world, ctx.selected_player_id, ctx.level, ctx.local_owner_id);
  GameStateRestorer::rebuild_entity_cache(
      &ctx.world, ctx.entity_cache, ctx.local_owner_id);

  if (ctx.selected_player_id != prev_selected_player) {
    effects.selected_player_changed = true;
  }

  effects.center_camera_on_local_forces = true;
  effects.troop_count_changed = true;
  effects.owner_info_changed = true;
  return effects;
}

namespace {

void apply_skirmish_ai_strategies(Engine::Core::World& world,
                                  const QSet<int>& owner_ids,
                                  int local_owner_id) {
  auto* ai_system = world.get_system<Game::Systems::AISystem>();
  if (ai_system == nullptr) {
    return;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  for (const int owner_id : owner_ids) {
    if (owner_id == local_owner_id || !owner_registry.is_ai(owner_id)) {
      continue;
    }

    ai_system->set_ai_strategy(owner_id,
                               Game::Systems::AI::AIStrategy::Expansionist,
                               k_skirmish_aggression,
                               k_skirmish_defense,
                               k_skirmish_harassment);
  }
}

} // namespace

auto MissionSetupCoordinator::apply_skirmish_commander_setup(
    const SkirmishCommanderSetupContext& ctx,
    const QVariantList& player_configs) const -> SkirmishCommanderSetupEffects {
  SkirmishCommanderSetupEffects effects;
  if (player_configs.isEmpty()) {
    return effects;
  }
  if (ctx.campaign != nullptr &&
      ctx.campaign->current_mission_context().is_campaign()) {
    return effects;
  }

  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    qWarning() << "Skirmish commander setup skipped: unit factory registry missing";
    return effects;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  auto& nation_registry = Game::Systems::NationRegistry::instance();

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path =
      Utils::Resources::resolve_resource_path(ctx.level.map_path);
  const bool map_loaded =
      Game::Map::MapLoader::load_from_json_file(resolved_map_path, map_def, &map_error);
  if (!map_loaded) {
    qWarning() << "Skirmish commander setup: failed to load map definition for"
               << ctx.level.map_path << "-" << map_error;
  }

  auto apply_team_color = [&](Engine::Core::Entity* entity, int owner_id) {
    if (entity == nullptr) {
      return;
    }
    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (renderable == nullptr) {
      return;
    }
    const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();
  };

  auto existing_owner_spawn_anchors = [&](int owner_id) {
    std::vector<App::Core::ExistingOwnerSpawnAnchor> anchors;
    auto entities = ctx.world.get_entities_with<Engine::Core::UnitComponent>();
    anchors.reserve(entities.size());
    for (auto* entity : entities) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->owner_id != owner_id ||
          unit->health <= 0) {
        continue;
      }
      anchors.push_back(
          {.position = {transform->position.x, transform->position.z},
           .is_building = Game::Units::is_building_spawn(unit->spawn_type)});
    }
    return anchors;
  };

  auto map_spawn_fallback =
      [&](int owner_id) -> std::optional<Game::Mission::Position> {
    if (!map_loaded) {
      return std::nullopt;
    }
    for (const auto& spawn : map_def.spawns) {
      if (spawn.player_id != owner_id) {
        continue;
      }
      float world_x = spawn.x;
      float world_z = spawn.z;
      if (map_def.coordSystem == Game::Map::CoordSystem::Grid) {
        const float tile = std::max(0.0001F, map_def.grid.tile_size);
        world_x = (spawn.x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
        world_z = (spawn.z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
      }
      return Game::Mission::Position{world_x, world_z};
    }
    return std::nullopt;
  };

  QSet<int> processed_owner_ids;
  for (const QVariant& config_var : player_configs) {
    const QVariantMap config = config_var.toMap();
    if (config.isEmpty()) {
      continue;
    }

    int owner_id = config.value("player_id", -1).toInt();
    if (config.value("isHuman", false).toBool()) {
      owner_id = ctx.local_owner_id;
    }
    if (owner_id < 0 || processed_owner_ids.contains(owner_id)) {
      continue;
    }
    processed_owner_ids.insert(owner_id);

    const auto* assigned_nation = nation_registry.get_nation_for_player(owner_id);
    const auto nation_id = assigned_nation != nullptr
                               ? assigned_nation->id
                               : nation_registry.default_nation_id();
    const QString nation_key =
        QString::fromStdString(Game::Systems::nation_id_to_string(nation_id));
    const auto configured_commander =
        config.contains("commanderTroop")
            ? std::optional<QString>(config.value("commanderTroop").toString())
            : std::nullopt;
    const QString commander_troop =
        App::Core::resolve_commander_troop(nation_key, configured_commander);
    if (commander_troop.isEmpty()) {
      continue;
    }

    const auto spawn_type =
        Game::Units::spawn_typeFromString(commander_troop.trimmed().toStdString());
    if (!spawn_type.has_value()) {
      qWarning() << "Skirmish commander setup: unknown commander troop"
                 << commander_troop;
      continue;
    }
    const auto troop_type = Game::Units::spawn_typeToTroopType(*spawn_type);
    if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
      qWarning() << "Skirmish commander setup: invalid commander troop"
                 << commander_troop;
      continue;
    }

    std::vector<Engine::Core::EntityID> existing_commanders;
    QVector3D existing_position{0.0F, 0.0F, 0.0F};
    bool has_existing_position = false;
    for (auto* entity :
         ctx.world.get_entities_with<Engine::Core::CommanderComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
        existing_commanders.push_back(entity->get_id());
        if (!has_existing_position) {
          if (const auto* xform =
                  entity->get_component<Engine::Core::TransformComponent>()) {
            existing_position =
                QVector3D(xform->position.x, xform->position.y, xform->position.z);
            has_existing_position = true;
          }
        }
      }
    }

    App::Core::ResolvedCommanderPosition commander_position;
    if (has_existing_position) {
      commander_position = {.position = {existing_position.x(), existing_position.z()},
                            .space = App::Core::CommanderPositionSpace::World};
    } else {
      const auto anchors = existing_owner_spawn_anchors(owner_id);
      if (!anchors.empty()) {
        commander_position =
            App::Core::resolve_commander_position({}, {}, anchors, {0.0F, 0.0F});
      } else if (const auto fallback = map_spawn_fallback(owner_id);
                 fallback.has_value()) {
        commander_position = {.position = fallback.value(),
                              .space = App::Core::CommanderPositionSpace::World};
      } else {
        commander_position = {.position = {0.0F, 0.0F},
                              .space = App::Core::CommanderPositionSpace::World};
      }
    }

    Game::Units::SpawnParams params;
    params.position =
        QVector3D(commander_position.position.x, 0.0F, commander_position.position.z);
    params.player_id = owner_id;
    params.spawn_type = *spawn_type;
    params.ai_controlled = owner_registry.is_ai(owner_id);
    params.nation_id = nation_id;
    auto unit = reg->create(params.spawn_type, ctx.world, params);
    if (!unit) {
      qWarning() << "Skirmish commander setup: failed to spawn commander"
                 << commander_troop << "for owner" << owner_id;
      continue;
    }

    for (const auto id : existing_commanders) {
      ctx.world.destroy_entity(id);
    }

    apply_team_color(ctx.world.get_entity(unit->id()), owner_id);
  }

  apply_skirmish_ai_strategies(ctx.world, processed_owner_ids, ctx.local_owner_id);

  return effects;
}

auto MissionSetupCoordinator::spawn_wave(const MissionWaveContext& ctx,
                                         const PendingMissionWave& wave) const
    -> MissionWaveEffects {
  MissionWaveEffects effects;
  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    qWarning() << "Mission wave spawn skipped: unit factory registry missing";
    return effects;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  if (owner_registry.get_owner_type(wave.owner_id) ==
      Game::Systems::OwnerType::Neutral) {
    owner_registry.register_owner_with_id(
        wave.owner_id,
        Game::Systems::OwnerType::AI,
        QCoreApplication::translate("MissionSetupCoordinator", "AI Wave %1")
            .arg(wave.owner_id)
            .toStdString());
  }

  const bool ai_controlled = owner_registry.is_ai(wave.owner_id);
  if (wave.composition.empty()) {
    qWarning() << "Mission wave has empty composition for AI" << wave.ai_id;
    return effects;
  }
  const float spacing = std::max(0.5F, ctx.level.tile_size * 1.2F);
  const int composition_count = static_cast<int>(wave.composition.size());
  const std::vector<QVector3D> entry_positions = wave.entry_positions();
  const auto entry_count = static_cast<int>(entry_positions.size());
  constexpr float k_group_radius_multiplier = 3.0F;
  constexpr float k_two_pi = 6.28318530718F;
  constexpr float k_elite_health_multiplier = 1.6F;

  int spawned_units = 0;

  for (int comp_index = 0; comp_index < composition_count; ++comp_index) {
    const auto& comp = wave.composition[static_cast<std::size_t>(comp_index)];
    const auto spawn_type = Game::Units::spawn_typeFromString(comp.type.toStdString());
    if (!spawn_type.has_value()) {
      qWarning() << "Mission wave: unknown unit type" << comp.type;
      continue;
    }

    const int count = std::max(1, comp.count);
    const int grid = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float angle = (k_two_pi * static_cast<float>(comp_index)) /
                        static_cast<float>(composition_count);
    const QVector3D entry = entry_positions[static_cast<std::size_t>(
        entry_count > 0 ? comp_index % entry_count : 0)];
    const QVector3D group_center =
        entry + QVector3D(std::cos(angle), 0.0F, std::sin(angle)) *
                    (spacing * k_group_radius_multiplier);

    for (int i = 0; i < count; ++i) {
      const int row = i / grid;
      const int col = i % grid;
      const float offset_x = (float(col) - (grid - 1) * 0.5F) * spacing;
      const float offset_z = (float(row) - (grid - 1) * 0.5F) * spacing;
      const QVector3D pos = QVector3D(
          group_center.x() + offset_x, group_center.y(), group_center.z() + offset_z);

      Game::Units::SpawnParams sp;
      sp.position = pos;
      sp.player_id = wave.owner_id;
      sp.spawn_type = spawn_type.value();
      sp.ai_controlled = ai_controlled;
      sp.nation_id = wave.nation_id;

      auto unit = reg->create(sp.spawn_type, ctx.world, sp);
      if (!unit) {
        qWarning() << "Mission wave: failed to spawn unit" << comp.type << "for owner"
                   << wave.owner_id;
        continue;
      }

      effects.spawned_entity_ids.push_back(unit->id());

      auto* entity = ctx.world.get_entity(unit->id());
      if (entity != nullptr) {
        if (ai_controlled) {
          auto* assault = entity->add_component<Engine::Core::AssaultWaveComponent>();
          assault->wave_phase = wave.phase_index;
          assault->has_march_target = true;
          assault->march_target_x = wave.defense_reference_world_position.x();
          assault->march_target_z = wave.defense_reference_world_position.z();
        }
        auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
        if (renderable != nullptr) {
          const QVector3D team_color = Game::Visuals::team_colorForOwner(wave.owner_id);
          renderable->color[0] = team_color.x();
          renderable->color[1] = team_color.y();
          renderable->color[2] = team_color.z();
        }
        if (comp.elite) {
          auto* unit_component = entity->get_component<Engine::Core::UnitComponent>();
          if (unit_component != nullptr) {
            unit_component->max_health = static_cast<int>(
                std::lround(unit_component->max_health * k_elite_health_multiplier));
            unit_component->health = unit_component->max_health;
          }
        }
      }
      spawned_units++;
    }
  }

  if (spawned_units > 0) {
    qInfo() << "Mission wave spawned for AI" << wave.ai_id << "(" << wave.owner_id
            << "):" << spawned_units << "units at t=" << ctx.campaign_mission_elapsed;

    order_wave_advance(ctx.world, wave, effects.spawned_entity_ids);

    const QString announcement =
        QCoreApplication::translate("MissionSetupCoordinator",
                                    "Assault phase %1/%2: %3 from the %4 (%5 units)")
            .arg(wave.phase_index)
            .arg(wave.phase_count)
            .arg(wave_display_name(wave), wave_direction(wave))
            .arg(spawned_units);
    effects.mission_announcements.append(announcement);
  }
  return effects;
}

auto build_pending_mission_waves(const MissionWaveBuildContext& ctx)
    -> std::vector<PendingMissionWave> {
  std::vector<PendingMissionWave> waves;

  Game::Map::MapDefinition map_def;
  QString map_error;
  bool map_loaded = false;
  if (!ctx.level.map_path.isEmpty()) {
    const QString resolved_map_path =
        Utils::Resources::resolve_resource_path(ctx.level.map_path);
    map_loaded = Game::Map::MapLoader::load_from_json_file(
        resolved_map_path, map_def, &map_error);
    if (!map_loaded) {
      qWarning() << "Mission wave build: failed to load map definition for"
                 << ctx.level.map_path << "-" << map_error;
    }
  }

  auto to_world = [&](const Game::Mission::Position& pos) {
    float world_x = pos.x;
    float world_z = pos.z;
    if (map_loaded && map_def.coordSystem == Game::Map::CoordSystem::Grid) {
      const float tile = std::max(0.0001F, map_def.grid.tile_size);
      world_x = (pos.x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
    } else if (!map_loaded) {
      const float tile = std::max(0.0001F, ctx.level.tile_size);
      world_x = (pos.x - (ctx.level.grid_width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (ctx.level.grid_height * 0.5F - 0.5F)) * tile;
    }
    return QVector3D(world_x, 0.0F, world_z);
  };

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  auto resolve_nation = [&nation_registry](const QString& nation_str) {
    const auto parsed = Game::Systems::nation_id_from_string(nation_str.toStdString());
    return parsed.value_or(nation_registry.default_nation_id());
  };

  const float mission_strength_multiplier =
      Game::Mission::difficulty_strength_multiplier(ctx.mission_difficulty);

  int ai_owner_id = 2;
  for (const auto& ai_setup : ctx.mission.ai_setups) {
    const float ai_strength_multiplier =
        mission_strength_multiplier *
        Game::Mission::difficulty_strength_multiplier(ai_setup.difficulty);
    const auto ai_nation_id = resolve_nation(ai_setup.nation);

    int wave_ordinal = 0;
    for (const auto& wave : ai_setup.waves) {
      PendingMissionWave pending_wave;
      pending_wave.owner_id = ai_owner_id;
      pending_wave.nation_id = ai_nation_id;
      pending_wave.ai_id = ai_setup.id;
      pending_wave.label = wave.label;
      pending_wave.trigger_time = std::max(0.0F, wave.timing);
      pending_wave.trigger = wave.trigger;
      pending_wave.grace_seconds = std::max(0.0F, wave.grace_seconds);
      pending_wave.warning_seconds = std::max(0.0F, wave.warning_seconds);
      pending_wave.authored_phase = wave.phase;
      pending_wave.defense_reference_world_position =
          ctx.defense_reference_world_position;
      pending_wave.clear_reward = wave.clear_reward;

      for (const auto& entry_point : wave.resolved_entry_points()) {
        pending_wave.entry_world_positions.push_back(to_world(entry_point));
      }
      pending_wave.entry_world_position =
          pending_wave.entry_world_positions.empty()
              ? QVector3D(0.0F, 0.0F, 0.0F)
              : pending_wave.entry_world_positions.front();

      const float escalation = 1.0F + (std::max(0.0F, ai_setup.wave_escalation) *
                                       static_cast<float>(wave_ordinal));
      const float strength =
          std::max(0.1F, wave.strength) * ai_strength_multiplier * escalation;

      std::vector<Game::Mission::WaveComposition> composition = wave.composition;
      if (!wave.archetype.isEmpty()) {
        auto expanded = Game::Mission::WaveArchetypeCatalog::instance().expand(
            wave.archetype, 1.0F);
        if (expanded.empty()) {
          qWarning() << "Mission wave for AI" << ai_setup.id
                     << "names unknown archetype" << wave.archetype;
        }
        composition.insert(composition.end(),
                           std::make_move_iterator(expanded.begin()),
                           std::make_move_iterator(expanded.end()));
      }
      pending_wave.composition =
          Game::Mission::scale_wave_composition(composition, strength);

      waves.push_back(std::move(pending_wave));
      wave_ordinal++;
    }

    ai_owner_id++;
  }

  assign_wave_phases(waves, 0);
  return waves;
}

auto build_pending_mission_events(const Game::Mission::MissionDefinition& mission)
    -> std::vector<PendingMissionEvent> {
  std::vector<PendingMissionEvent> events;

  for (const auto& game_event : mission.events) {
    if (game_event.trigger.type != QLatin1String("timer") ||
        !game_event.trigger.time.has_value()) {
      qWarning() << "Mission" << mission.id << "uses unsupported event trigger"
                 << game_event.trigger.type << "- skipping";
      continue;
    }

    for (const auto& action : game_event.actions) {
      if (action.type != QLatin1String("show_message")) {
        qWarning() << "Mission" << mission.id << "uses unsupported event action"
                   << action.type << "- skipping";
        continue;
      }
      if (!action.text.has_value() || action.text->isEmpty()) {
        continue;
      }
      events.push_back({.trigger_time = *game_event.trigger.time,
                        .text = *action.text,
                        .fired = false});
    }
  }

  std::stable_sort(events.begin(),
                   events.end(),
                   [](const PendingMissionEvent& a, const PendingMissionEvent& b) {
                     return a.trigger_time < b.trigger_time;
                   });
  return events;
}

} // namespace App::Core
