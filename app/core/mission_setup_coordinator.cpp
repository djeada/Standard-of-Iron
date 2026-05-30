#include "mission_setup_coordinator.h"

#include <QDebug>
#include <QSet>
#include <QVariantMap>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
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
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
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
    return x >= 0.0F ? QStringLiteral("east") : QStringLiteral("west");
  }
  if (az > ax * k_direction_bias) {
    return z >= 0.0F ? QStringLiteral("south") : QStringLiteral("north");
  }
  if (x >= 0.0F && z >= 0.0F) {
    return QStringLiteral("southeast");
  }
  if (x >= 0.0F && z < 0.0F) {
    return QStringLiteral("northeast");
  }
  if (x < 0.0F && z >= 0.0F) {
    return QStringLiteral("southwest");
  }
  return QStringLiteral("northwest");
}

} // namespace

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
        sp.ai_controlled = ai_controlled;
        sp.nation_id = nation_id;

        auto unit = reg->create(sp.spawn_type, ctx.world, sp);
        if (!unit) {
          qWarning() << "Mission setup: failed to spawn unit" << unit_setup.type
                     << "for owner" << owner_id;
          continue;
        }
        apply_team_color(ctx.world.get_entity(unit->id()), owner_id);
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
    owner_registry.register_owner_with_id(local_owner_id,
                                          Game::Systems::OwnerType::Player,
                                          "Player " + std::to_string(local_owner_id));
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

  int ai_owner_id = 2;
  int default_team_id = 1;
  for (const auto& ai_setup : mission.ai_setups) {
    if (owner_registry.get_owner_type(ai_owner_id) ==
        Game::Systems::OwnerType::Neutral) {
      owner_registry.register_owner_with_id(ai_owner_id,
                                            Game::Systems::OwnerType::AI,
                                            "AI Player " + std::to_string(ai_owner_id));
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

    for (const auto& wave : ai_setup.waves) {
      PendingMissionWave pending_wave;
      pending_wave.owner_id = ai_owner_id;
      pending_wave.nation_id = ai_nation_id;
      pending_wave.ai_id = ai_setup.id;
      pending_wave.trigger_time = std::max(0.0F, wave.timing);
      pending_wave.entry_world_position = mission_position_to_world(wave.entry_point);
      pending_wave.composition = wave.composition;
      ctx.pending_waves.push_back(std::move(pending_wave));
    }

    ai_owner_id++;
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
      Game::Systems::AI::AIStrategy strategy = Game::Systems::AI::AIStrategy::Balanced;

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

  auto owner_has_living_commander = [&](int owner_id) {
    for (auto* entity :
         ctx.world.get_entities_with<Engine::Core::CommanderComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
        return true;
      }
    }
    return false;
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
    if (owner_id < 0 || processed_owner_ids.contains(owner_id) ||
        owner_has_living_commander(owner_id)) {
      continue;
    }
    processed_owner_ids.insert(owner_id);

    const auto* assigned_nation = nation_registry.get_nation_for_player(owner_id);
    const auto nation_id = assigned_nation != nullptr
                               ? assigned_nation->id
                               : nation_registry.default_nation_id();
    QString nation_key = config.value("nationId").toString();
    if (nation_key.isEmpty()) {
      nation_key =
          QString::fromStdString(Game::Systems::nation_id_to_string(nation_id));
    }
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

    App::Core::ResolvedCommanderPosition commander_position;
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
    apply_team_color(ctx.world.get_entity(unit->id()), owner_id);
  }
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
    owner_registry.register_owner_with_id(wave.owner_id,
                                          Game::Systems::OwnerType::AI,
                                          "AI Wave " + std::to_string(wave.owner_id));
  }

  const bool ai_controlled = owner_registry.is_ai(wave.owner_id);
  if (wave.composition.empty()) {
    qWarning() << "Mission wave has empty composition for AI" << wave.ai_id;
    return effects;
  }
  const float spacing = std::max(0.5F, ctx.level.tile_size * 1.2F);
  const int composition_count = static_cast<int>(wave.composition.size());
  constexpr float k_group_radius_multiplier = 3.0F;
  constexpr float k_two_pi = 6.28318530718F;

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
    const QVector3D group_center =
        wave.entry_world_position + QVector3D(std::cos(angle), 0.0F, std::sin(angle)) *
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

      auto* entity = ctx.world.get_entity(unit->id());
      if (entity != nullptr) {
        auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
        if (renderable != nullptr) {
          const QVector3D team_color = Game::Visuals::team_colorForOwner(wave.owner_id);
          renderable->color[0] = team_color.x();
          renderable->color[1] = team_color.y();
          renderable->color[2] = team_color.z();
        }
      }
      spawned_units++;
    }
  }

  if (spawned_units > 0) {
    qInfo() << "Mission wave spawned for AI" << wave.ai_id << "(" << wave.owner_id
            << "):" << spawned_units << "units at t=" << ctx.campaign_mission_elapsed;

    QString wave_name = wave.ai_id;
    wave_name.replace('_', ' ');
    if (wave_name.isEmpty()) {
      wave_name = QStringLiteral("Enemy");
    }

    const QString direction = classify_wave_direction(wave.entry_world_position);
    const QString announcement = QStringLiteral("%1 wave from the %2 (%3 units)")
                                     .arg(wave_name, direction)
                                     .arg(spawned_units);
    effects.mission_announcements.append(announcement);
  }
  return effects;
}

} // namespace App::Core
