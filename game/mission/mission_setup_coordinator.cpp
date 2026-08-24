#include "game/mission/mission_setup_coordinator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSet>
#include <QVariantMap>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/mission_context.h"
#include "game/map/wave_archetype_catalog.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_waves.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/world_restore.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "utils/resource_utils.h"

namespace Game::Mission {
namespace {

constexpr float k_skirmish_aggression = 0.6F;
constexpr float k_skirmish_defense = 0.55F;
constexpr float k_skirmish_harassment = 0.5F;

} // namespace

namespace {

auto is_scenario_controlled_behavior(Game::Mission::UnitBehavior behavior) -> bool {
  return behavior == Game::Mission::UnitBehavior::Guard ||
         behavior == Game::Mission::UnitBehavior::Hold ||
         behavior == Game::Mission::UnitBehavior::Patrol;
}

} // namespace

auto mission_position_to_world(const Game::Mission::Position& position,
                               const Game::Map::MapDefinition* map_def,
                               const Game::Systems::LevelSnapshot& level) -> QVector3D {
  float world_x = position.x;
  float world_z = position.z;
  if (map_def != nullptr) {
    if (map_def->coordSystem == Game::Map::CoordSystem::Grid) {
      const float tile = std::max(0.0001F, map_def->grid.tile_size);
      world_x = (position.x - (map_def->grid.width * 0.5F - 0.5F)) * tile;
      world_z = (position.z - (map_def->grid.height * 0.5F - 0.5F)) * tile;
    }
  } else {
    const float tile = std::max(0.0001F, level.tile_size);
    world_x = (position.x - (level.grid_width * 0.5F - 0.5F)) * tile;
    world_z = (position.z - (level.grid_height * 0.5F - 0.5F)) * tile;
  }
  return {world_x, 0.0F, world_z};
}

auto make_mission_position_to_world(const Game::Systems::LevelSnapshot& level)
    -> Game::Mission::MissionPositionToWorld {
  auto map_def = std::make_shared<Game::Map::MapDefinition>();
  bool map_loaded = false;
  if (!level.map_path.isEmpty()) {
    QString map_error;
    const QString resolved = Utils::Resources::resolve_resource_path(level.map_path);
    map_loaded =
        Game::Map::MapLoader::load_from_json_file(resolved, *map_def, &map_error);
    if (!map_loaded) {
      qWarning() << "Mission stages: failed to load map definition for"
                 << level.map_path << "-" << map_error;
    }
  }

  return [map_def, map_loaded, level](const Game::Mission::Position& position) {
    return mission_position_to_world(
        position, map_loaded ? map_def.get() : nullptr, level);
  };
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
  auto& session = Game::Session::session_for(ctx.world);
  auto& owner_registry = session.owners();
  auto& nation_registry = session.nations();

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
    auto entities = ctx.world.collect_entities_with<Engine::Core::UnitComponent>();
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

  auto position_to_world = [&](const Game::Mission::Position& pos) {
    return mission_position_to_world(pos, map_loaded ? &map_def : nullptr, ctx.level);
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
      const QVector3D base_pos = position_to_world(unit_setup.position);
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
            const QVector3D waypoint = position_to_world(authored_waypoint);
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

  const auto map_commanders = Game::Mission::commander_troops_by_owner(map_def);

  auto verify_owner_commander = [&](int owner_id, const QString& force_label) {
    for (auto* entity :
         ctx.world.collect_entities_with<Engine::Core::CommanderComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
        return;
      }
    }
    if (map_commanders.find(owner_id) == map_commanders.end()) {
      qWarning() << "Mission setup:" << ctx.level.map_path << "authors no commander for"
                 << force_label << "(owner" << owner_id
                 << ") - that force starts headless";
      return;
    }
    qWarning() << "Mission setup: map commander for" << force_label << "(owner"
               << owner_id << ") failed to spawn";
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

          const QVector3D pos = position_to_world(building_setup.position);

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

  verify_owner_commander(local_owner_id, QStringLiteral("the player"));
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

    verify_owner_commander(ai_owner_id, ai_setup.id);
    spawn_units_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_units);
    spawn_buildings_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_buildings);

    ai_owner_id++;
  }

  {
    auto built = build_pending_mission_waves(
        {.mission = mission,
         .mission_difficulty = ctx.campaign.current_mission_context().difficulty,
         .level = ctx.level,
         .nations = nation_registry,
         .defense_reference_world_position = defense_reference_world_position});
    ctx.pending_waves.insert(ctx.pending_waves.end(),
                             std::make_move_iterator(built.begin()),
                             std::make_move_iterator(built.end()));
  }

  if (auto* ai_system = ctx.world.get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();

    int ai_id = 2;
    for (const auto& ai_setup : mission.ai_setups) {
      Game::Systems::AI::AIPlayerProfile profile;
      profile.strategy = Game::Systems::AI::AIStrategy::Defensive;
      if (ai_setup.strategy.has_value()) {
        profile.strategy = Game::Systems::AI::AIStrategyFactory::parse_strategy(
            ai_setup.strategy.value());
      }
      profile.posture = Game::Systems::AI::AIStrategyFactory::parse_posture(
          ai_setup.posture.value_or(QString()), Game::Systems::AI::AIPosture::Garrison);
      profile.personality.aggression = ai_setup.personality.aggression;
      profile.personality.defense = ai_setup.personality.defense;
      profile.personality.harassment = ai_setup.personality.harassment;
      profile.difficulty = ai_setup.difficulty;

      ai_system->set_ai_profile(ai_id, profile);
      ai_id++;
    }
  }

  const auto restored =
      Game::Persistence::rebuild_registries_after_load(&ctx.world, ctx.local_owner_id);
  ctx.level.player_unit_id = restored.player_unit_id;
  effects.rebuild_entity_cache = true;

  if (ctx.selected_player_id != ctx.local_owner_id) {
    ctx.selected_player_id = ctx.local_owner_id;
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

  auto& owner_registry = Game::Session::session_for(world).owners();
  for (const int owner_id : owner_ids) {
    if (owner_id == local_owner_id || !owner_registry.is_ai(owner_id)) {
      continue;
    }

    Game::Systems::AI::AIPlayerProfile profile;
    profile.strategy = Game::Systems::AI::AIStrategy::Expansionist;
    profile.posture = Game::Systems::AI::AIPosture::Field;
    profile.personality.aggression = k_skirmish_aggression;
    profile.personality.defense = k_skirmish_defense;
    profile.personality.harassment = k_skirmish_harassment;
    ai_system->set_ai_profile(owner_id, profile);
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

  auto& session = Game::Session::session_for(ctx.world);
  auto& owner_registry = session.owners();
  auto& nation_registry = session.nations();

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

  auto existing_owner_spawn_anchors = [&](int owner_id) {
    std::vector<Game::Mission::ExistingOwnerSpawnAnchor> anchors;
    auto entities = ctx.world.collect_entities_with<Engine::Core::UnitComponent>();
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
        Game::Mission::resolve_commander_troop(nation_key, configured_commander);
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

    struct ExistingCommander {
      Engine::Core::EntityID id = 0;
      int health = 0;
    };
    std::vector<ExistingCommander> existing_commanders;
    QVector3D existing_position{0.0F, 0.0F, 0.0F};
    bool has_existing_position = false;
    for (auto* entity :
         ctx.world.collect_entities_with<Engine::Core::CommanderComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
        existing_commanders.push_back({entity->get_id(), unit->health});
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

    Game::Mission::ResolvedCommanderPosition commander_position;
    if (has_existing_position) {
      commander_position = {.position = {existing_position.x(), existing_position.z()},
                            .space = Game::Mission::CommanderPositionSpace::World};
    } else {
      const auto anchors = existing_owner_spawn_anchors(owner_id);
      if (!anchors.empty()) {
        commander_position =
            Game::Mission::resolve_commander_position({}, {}, anchors, {0.0F, 0.0F});
      } else if (const auto fallback = map_spawn_fallback(owner_id);
                 fallback.has_value()) {
        commander_position = {.position = fallback.value(),
                              .space = Game::Mission::CommanderPositionSpace::World};
      } else {
        commander_position = {.position = {0.0F, 0.0F},
                              .space = Game::Mission::CommanderPositionSpace::World};
      }
    }

    Game::Units::SpawnParams params;
    params.position =
        QVector3D(commander_position.position.x, 0.0F, commander_position.position.z);
    params.player_id = owner_id;
    params.spawn_type = *spawn_type;
    params.ai_controlled = owner_registry.is_ai(owner_id);
    params.nation_id = nation_id;

    for (const auto& existing : existing_commanders) {
      if (auto* existing_unit =
              ctx.world.try_get<Engine::Core::UnitComponent>(existing.id)) {
        existing_unit->health = 0;
      }
    }
    auto unit = reg->create(params.spawn_type, ctx.world, params);
    if (!unit) {
      for (const auto& existing : existing_commanders) {
        if (auto* existing_unit =
                ctx.world.try_get<Engine::Core::UnitComponent>(existing.id)) {
          existing_unit->health = existing.health;
        }
      }
      qWarning() << "Skirmish commander setup: failed to spawn commander"
                 << commander_troop << "for owner" << owner_id;
      continue;
    }

    for (const auto& existing : existing_commanders) {
      ctx.world.destroy_entity(existing.id);
    }
  }

  apply_skirmish_ai_strategies(ctx.world, processed_owner_ids, ctx.local_owner_id);

  return effects;
}

} // namespace Game::Mission
