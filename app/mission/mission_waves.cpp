#include "app/mission/mission_waves.h"

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

#include "app/mission/campaign_manager.h"
#include "app/mission/mission_commander_setup.h"
#include "app/mission/mission_setup_coordinator.h"
#include "app/persistence/game_state_restorer.h"
#include "game/command/command_queue.h"
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
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/visuals/team_colors.h"
#include "utils/resource_utils.h"

namespace App::Core {

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

  const QVector3D target = Game::Systems::NavGrid::snap_to_walkable_ground(
      from + (offset * k_approach_fraction));

  const auto plan =
      Game::Systems::CommandService::plan_ground_move(world, units, target);
  if (plan.positions.size() != units.size()) {
    return;
  }

  Game::Command::Move move;
  move.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  move.units = units;
  move.targets = plan.positions;
  Game::Command::submit(
      world, Game::Command::Source::Script, wave.owner_id, std::move(move));
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

auto MissionWaves::spawn(const MissionWaveContext& ctx,
                         const PendingMissionWave& wave) const -> MissionWaveEffects {
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
    return mission_position_to_world(pos, map_loaded ? &map_def : nullptr, ctx.level);
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
