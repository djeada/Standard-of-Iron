#include "game/mission/difficulty_forces.h"

#include <QDebug>
#include <QVector3D>

#include <algorithm>
#include <map>
#include <vector>

#include "game/core/component_combat.h"
#include "game/core/component_commander.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/map_transformer.h"
#include "game/mission/spawn_placement.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace Game::Mission {
namespace {

struct Conscript {
  Engine::Core::EntityID id = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  QVector3D position;
  bool ai_controlled = false;
};

void copy_standing_orders(Engine::Core::World& world,
                          Engine::Core::EntityID source,
                          Engine::Core::EntityID clone,
                          const QVector3D& position) {
  if (const auto* guard = world.try_get<Engine::Core::GuardModeComponent>(source)) {
    auto* copy = world.try_get<Engine::Core::GuardModeComponent>(clone);
    if (copy == nullptr) {
      copy = world.emplace<Engine::Core::GuardModeComponent>(clone);
    }
    if (copy != nullptr) {
      copy->active = guard->active;
      copy->guarded_entity_id = 0;
      copy->guard_position_x = position.x();
      copy->guard_position_z = position.z();
      copy->guard_radius = guard->guard_radius;
      copy->has_guard_target = guard->has_guard_target;
      copy->returning_to_guard_position = false;
    }
  }

  if (const auto* hold = world.try_get<Engine::Core::HoldModeComponent>(source)) {
    auto* copy = world.try_get<Engine::Core::HoldModeComponent>(clone);
    if (copy == nullptr) {
      copy = world.emplace<Engine::Core::HoldModeComponent>(clone);
    }
    if (copy != nullptr) {
      copy->active = hold->active;
    }
  }

  if (const auto* patrol = world.try_get<Engine::Core::PatrolComponent>(source)) {
    if (patrol->waypoints.size() < 2U) {
      return;
    }
    auto* copy = world.try_get<Engine::Core::PatrolComponent>(clone);
    if (copy == nullptr) {
      copy = world.emplace<Engine::Core::PatrolComponent>(clone);
    }
    if (copy != nullptr) {
      copy->waypoints = patrol->waypoints;
      copy->waypoints.front() = {position.x(), position.z()};
      copy->current_waypoint = 1U;
      copy->patrolling = patrol->patrolling;
    }
  }
}

auto is_eligible_conscript(const Engine::Core::World& world,
                           Engine::Core::EntityID id,
                           const Engine::Core::UnitComponent& unit) -> bool {
  if (unit.health <= 0) {
    return false;
  }
  if (!Game::Units::is_troop_spawn(unit.spawn_type)) {
    return false;
  }
  if (world.has<Engine::Core::BuildingComponent>(id)) {
    return false;
  }
  return !world.has<Engine::Core::CommanderComponent>(id);
}

auto live_troops_of(Engine::Core::World& world, int owner_id) -> int {
  int troops = 0;
  for (auto [id, unit] : world.view<const Engine::Core::UnitComponent>()) {
    if (unit.owner_id != owner_id) {
      continue;
    }
    if (unit.health <= 0 || !Game::Units::is_troop_spawn(unit.spawn_type)) {
      continue;
    }
    if (world.has<Engine::Core::BuildingComponent>(id)) {
      continue;
    }
    ++troops;
  }
  return troops;
}

} // namespace

auto difficulty_applies_to(const MatchDifficulty& difficulty,
                           const Game::Systems::OwnerRegistry& owners,
                           int owner_id,
                           int local_owner_id) -> bool {
  if (!owners.is_ai(owner_id)) {
    return false;
  }
  if (difficulty.has_owner(owner_id)) {
    return true;
  }
  if (owner_id == local_owner_id) {
    return false;
  }
  return owners.are_enemies(owner_id, local_owner_id);
}

auto apply_starting_force_difficulty(Engine::Core::World& world,
                                     const MatchDifficulty& difficulty,
                                     int local_owner_id) -> DifficultyForceResult {
  DifficultyForceResult result;
  if (difficulty.is_baseline()) {
    return result;
  }

  auto& owner_registry = Game::Session::session_for(world).owners();

  std::map<int, std::vector<Conscript>> by_owner;
  for (auto [id, unit, transform] :
       world.view<const Engine::Core::UnitComponent,
                  const Engine::Core::TransformComponent>()) {
    const int owner_id = unit.owner_id;
    if (!difficulty_applies_to(difficulty, owner_registry, owner_id, local_owner_id)) {
      continue;
    }
    if (!is_eligible_conscript(world, id, unit)) {
      continue;
    }
    by_owner[owner_id].push_back(
        {.id = id,
         .spawn_type = unit.spawn_type,
         .nation_id = unit.nation_id,
         .position = QVector3D(
             transform.position.x, transform.position.y, transform.position.z),
         .ai_controlled = world.has<Engine::Core::AIControlledComponent>(id)});
  }

  if (by_owner.empty()) {
    return result;
  }

  auto registry = Game::Map::MapTransformer::get_factory_registry();
  const int troop_cap = Game::GameConfig::instance().get_max_troops_per_player();

  for (auto& [owner_id, troops] : by_owner) {
    const DifficultyProfile profile = difficulty.profile_for(owner_id);
    if (profile.is_baseline() || troops.empty()) {
      continue;
    }

    const auto authored = static_cast<int>(troops.size());
    const int target = scaled_force_count(authored, profile.starting_unit_multiplier);
    if (target == authored) {
      continue;
    }
    ++result.owners_scaled;

    if (target < authored) {
      std::map<Game::Units::SpawnType, int> remaining;
      for (const auto& troop : troops) {
        ++remaining[troop.spawn_type];
      }
      int to_withdraw = authored - target;
      for (auto it = troops.rbegin(); it != troops.rend() && to_withdraw > 0; ++it) {
        if (remaining[it->spawn_type] <= 1) {
          continue;
        }
        --remaining[it->spawn_type];
        world.destroy_entity(it->id);
        --to_withdraw;
        ++result.units_withdrawn;
      }
      qInfo() << "Difficulty: owner" << owner_id << "team"
              << owner_registry.get_owner_team(owner_id) << "preset"
              << difficulty.id_for(owner_id) << "baseline" << authored
              << "troops, withdrew" << (authored - target - to_withdraw) << "of"
              << (authored - target);
      continue;
    }

    if (!registry) {
      qWarning() << "Difficulty: no unit factory registry; owner" << owner_id
                 << "keeps its authored force";
      continue;
    }

    int requested = target - authored;
    const int room = troop_cap - live_troops_of(world, owner_id);
    int capped_by_population = 0;
    if (requested > room) {
      capped_by_population = requested - std::max(0, room);
      requested = std::max(0, room);
    }

    int added = 0;
    int unplaced = 0;
    for (int i = 0; i < requested; ++i) {
      const Conscript& source = troops[static_cast<std::size_t>(i) % troops.size()];

      Game::Units::SpawnParams params;
      params.position = source.position;
      params.player_id = owner_id;
      params.spawn_type = source.spawn_type;
      params.ai_controlled = source.ai_controlled;
      params.nation_id = source.nation_id;

      auto unit = registry->create(params.spawn_type, world, params);
      if (!unit) {
        qWarning() << "Difficulty: failed to reinforce owner" << owner_id << "with"
                   << Game::Units::spawn_typeToQString(source.spawn_type);
        ++unplaced;
        continue;
      }

      const Engine::Core::EntityID clone = unit->id();
      const auto placed = place_clear_of_units(
          world, clone, source.position, BuildingFootprints::Refuse);
      if (!placed.has_value()) {

        world.destroy_entity(clone);
        ++unplaced;
        continue;
      }
      copy_standing_orders(world, source.id, clone, *placed);
      ++added;
    }

    result.units_added += added;
    result.units_capped += unplaced + capped_by_population;

    qInfo() << "Difficulty: owner" << owner_id << "team"
            << owner_registry.get_owner_team(owner_id) << "preset"
            << difficulty.id_for(owner_id) << "baseline" << authored
            << "troops, requested" << (target - authored) << "reinforcements, placed"
            << added
            << (capped_by_population > 0
                    ? "- short of the population cap"
                    : (unplaced > 0 ? "- short of clear ground" : ""));
  }

  return result;
}

auto apply_undead_wave_difficulty(Engine::Core::World& world,
                                  const MatchDifficulty& difficulty) -> float {
  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  if (undead == nullptr) {
    return 1.0F;
  }
  const float multiplier = resolve_difficulty(difficulty.baseline_id()).wave_multiplier;
  undead->set_wave_multiplier(multiplier);
  return multiplier;
}

} // namespace Game::Mission
