#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

void ArenaScenarioRunner::Impl::record_animal(
    Engine::Core::Entity& entity,
    const Engine::Core::UnitComponent& unit,
    const Engine::Core::WildlifeComponent& wildlife,
    TraceFrame& frame) {
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }
  auto const behavior_name = [&]() -> QString {
    switch (wildlife.behavior) {
    case Game::Wildlife::Behavior::Graze:
      return QStringLiteral("graze");
    case Game::Wildlife::Behavior::Flee:
      return QStringLiteral("flee");
    case Game::Wildlife::Behavior::Stalk:
      return QStringLiteral("stalk");
    case Game::Wildlife::Behavior::Roam:
      return QStringLiteral("roam");
    default:
      return QStringLiteral("other");
    }
  }();

  TraceAnimal animal;
  animal.entity_id = entity.get_id();
  animal.position = vector_from_transform(*transform);
  animal.health = unit.health;
  animal.species = wildlife.species == Game::Wildlife::Species::Wolf
                       ? QStringLiteral("wolf")
                       : QStringLiteral("sheep");
  animal.behavior = behavior_name;
  animal.focus_id = wildlife.focus_id;
  animal.yaw = transform->rotation.y;
  animal.desired_yaw = transform->desired_yaw;
  animal.has_desired_yaw = transform->has_desired_yaw;
  if (auto const* movement = entity.get_component<Engine::Core::MovementComponent>()) {
    animal.vx = movement->get_vx();
    animal.vz = movement->get_vz();
    animal.has_move_target = movement->get_has_target();
    animal.goal_x = movement->get_goal_x();
    animal.goal_z = movement->get_goal_y();
  }
  auto* const registry = entity.registry();
  if (auto const* attack =
          registry == nullptr
              ? nullptr
              : registry->try_get<Engine::Core::AttackComponent>(entity.get_id())) {
    animal.melee_lock = attack->in_melee_lock;
  }
  animal.state_timer = wildlife.state_timer;
  animal.stall_timer = wildlife.stall_timer;
  animal.staggered = registry != nullptr &&
                     registry->has<Engine::Core::StaggerComponent>(entity.get_id());
  animal.biting = wildlife.bite_timer > 0.0F;
  animal.bite_phase =
      animal.biting
          ? 1.0F - wildlife.bite_timer /
                       Engine::Core::WildlifeComponent::k_bite_animation_seconds
          : -1.0F;
  animal.flinch_phase =
      wildlife.flinch_timer > 0.0F
          ? 1.0F - wildlife.flinch_timer /
                       Engine::Core::WildlifeComponent::k_flinch_animation_seconds
          : -1.0F;
  animal.bite_target_id = wildlife.bite_target_id;
  animal.impact_pending = wildlife.bite_impact_pending;
  animal.dying = entity.has_component<Engine::Core::DeathAnimationComponent>();
  frame.animals.push_back(std::move(animal));
}

void ArenaScenarioRunner::Impl::record_animals(TraceFrame& frame) {
  for (auto* entity : world.collect_entities_with<Engine::Core::WildlifeComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto const* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    if (unit == nullptr || wildlife == nullptr) {
      continue;
    }
    record_animal(*entity, *unit, *wildlife, frame);
  }
}

void ArenaScenarioRunner::Impl::observe_wildlife() {
  int population = 0;
  for (auto [id, unit, wildlife] :
       world.view<Engine::Core::UnitComponent, Engine::Core::WildlifeComponent>()) {
    if (unit.health <= 0) {
      continue;
    }
    ++population;
    switch (wildlife.behavior) {
    case Game::Wildlife::Behavior::Graze:
      wildlife_observation.grazing_seen = true;
      break;
    case Game::Wildlife::Behavior::Flee:
      wildlife_observation.flee_seen = true;
      break;
    case Game::Wildlife::Behavior::Stalk:
      wildlife_observation.hunt_seen = true;
      break;
    default:
      break;
    }
  }

  for (auto const& bird : Game::Wildlife::BirdFlockManager::instance().birds()) {
    ++population;
    if (bird.behavior == Game::Wildlife::Behavior::Scatter) {
      wildlife_observation.flee_seen = true;
    }
  }
  wildlife_observation.bird_scatter_events =
      Game::Wildlife::BirdFlockManager::instance().stats().scatter_events;
  wildlife_observation.bird_flyovers =
      Game::Wildlife::BirdFlockManager::instance().stats().flyovers_launched;

  wildlife_observation.peak_population =
      std::max(wildlife_observation.peak_population, population);
  wildlife_observation.min_population =
      wildlife_observation.min_population < 0
          ? population
          : std::min(wildlife_observation.min_population, population);
}

void ArenaScenarioRunner::Impl::initialize_battle_sides() {
  battle_sides.clear();
  if (scenario.battle_sides.empty()) {
    return;
  }

  for (auto const& side : scenario.battle_sides) {
    BattleSideState state;
    state.owner_id = side.owner_id;
    state.home_radius = side.home_radius;
    state.label = side.label.isEmpty() ? QStringLiteral("owner_%1").arg(side.owner_id)
                                       : side.label;
    state.home = world_origin + side.home;
    battle_sides.push_back(std::move(state));
  }
  for (auto& state : battle_sides) {
    QVector3D enemy_sum;
    int enemy_count = 0;
    for (auto const& other : battle_sides) {
      if (other.owner_id == state.owner_id) {
        continue;
      }
      enemy_sum += other.home;
      ++enemy_count;
    }
    if (enemy_count == 0) {
      continue;
    }
    state.enemy_home = enemy_sum / static_cast<float>(enemy_count);
    state.separation = horizontal_distance(state.home, state.enemy_home);
    state.has_axis = state.separation > 1.0F;
  }
  report.battle.tracked = !battle_sides.empty();
  observe_battle();
  for (auto& state : battle_sides) {
    state.initial_units = state.living_units;
    state.initial_soldiers = state.living_soldiers;
    state.initial_buildings = state.seen_buildings;
  }
}

void ArenaScenarioRunner::Impl::observe_battle() {
  if (battle_sides.empty()) {
    return;
  }

  struct Accumulator {
    int units{0};

    int soldiers{0};
    int buildings{0};
    QVector3D army_sum;
    int army_count{0};
    int home_units{0};
    int forward_units{0};
  };
  std::vector<Accumulator> accumulators(battle_sides.size());

  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto const* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    std::size_t index = battle_sides.size();
    for (std::size_t i = 0; i < battle_sides.size(); ++i) {
      if (battle_sides[i].owner_id == unit->owner_id) {
        index = i;
        break;
      }
    }
    if (index >= battle_sides.size()) {
      continue;
    }

    auto& side = battle_sides[index];
    auto& accumulator = accumulators[index];
    if (Game::Units::is_building_spawn(unit->spawn_type)) {
      ++accumulator.buildings;
      if (!side.seen_buildings.contains(entity->get_id())) {
        side.seen_buildings.insert(entity->get_id());
        side.building_census[QString::fromStdString(
            Game::Units::spawn_typeToString(unit->spawn_type))] += 1;
      }
      continue;
    }

    ++accumulator.units;
    accumulator.soldiers += Game::Systems::FormationCombat::living_slot_count(
        *entity, Game::Systems::FormationCombat::resolve_definition(*unit).total_count);
    side.seen_units.insert(entity->get_id());
    if (unit->spawn_type == Game::Units::SpawnType::Builder ||
        entity->has_component<Engine::Core::CommanderComponent>()) {
      continue;
    }
    auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      QVector3D const position = vector_from_transform(*transform);
      accumulator.army_sum += position;
      ++accumulator.army_count;
      if (horizontal_distance(position, side.home) <= side.home_radius) {
        ++accumulator.home_units;
      }
      if (side.has_axis) {
        QVector3D axis = side.enemy_home - side.home;
        axis.setY(0.0F);
        QVector3D offset = position - side.home;
        offset.setY(0.0F);
        if (QVector3D::dotProduct(offset, axis.normalized()) / side.separation > 0.5F) {
          ++accumulator.forward_units;
        }
      }
    }
  }

  for (std::size_t i = 0; i < battle_sides.size(); ++i) {
    auto& side = battle_sides[i];
    auto const& accumulator = accumulators[i];
    if (host.sample_ai_doctrine) {
      auto const doctrine = host.sample_ai_doctrine(side.owner_id);
      if (doctrine.valid) {
        side.strategy = doctrine.strategy;
        side.posture = doctrine.posture;
        side.ai_state = doctrine.state;
        side.wave_committed = doctrine.wave_committed;
        side.wave_size = doctrine.wave_size;
        float const step = std::max(0.0F, elapsed - side.seconds_observed);
        side.seconds_observed = elapsed;
        if (doctrine.state == QStringLiteral("attacking")) {
          side.seconds_attacking += step;
        }
      }
    }
    side.living_units = accumulator.units;
    side.living_soldiers = accumulator.soldiers;
    side.living_buildings = accumulator.buildings;
    side.peak_units = std::max(side.peak_units, accumulator.units);
    side.peak_soldiers = std::max(side.peak_soldiers, accumulator.soldiers);
    side.peak_buildings = std::max(side.peak_buildings, accumulator.buildings);
    side.peak_home_units = std::max(side.peak_home_units, accumulator.home_units);
    side.peak_forward_units =
        std::max(side.peak_forward_units, accumulator.forward_units);
    if (accumulator.army_count > 0) {
      side.home_share_sum += static_cast<double>(accumulator.home_units) /
                             static_cast<double>(accumulator.army_count);
      ++side.home_share_samples;
    }
    if (accumulator.units > 0 || accumulator.buildings > 0) {
      side.had_presence = true;
    } else if (side.had_presence && side.eliminated_at < 0.0F) {
      side.eliminated_at = elapsed;
    }

    if (side.has_axis && accumulator.army_count > 0) {
      QVector3D const centroid =
          accumulator.army_sum / static_cast<float>(accumulator.army_count);
      QVector3D axis = side.enemy_home - side.home;
      axis.setY(0.0F);
      QVector3D offset = centroid - side.home;
      offset.setY(0.0F);
      float const projected =
          QVector3D::dotProduct(offset, axis.normalized()) / side.separation;
      side.final_advance = projected;
      side.peak_advance =
          side.has_advance ? std::max(side.peak_advance, projected) : projected;
      side.has_advance = true;
      side.advance_samples.emplace_back(elapsed, projected);
    }
  }

  if (!battle_decided) {
    int survivors = 0;
    for (auto const& side : battle_sides) {
      if (side.eliminated_at < 0.0F) {
        ++survivors;
      }
    }
    if (survivors <= 1 && battle_sides.size() >= 2U) {
      battle_decided = true;
      battle_decided_at = elapsed;
    }
  }
}

auto ArenaScenarioRunner::Impl::battle_decision_ends_scenario() const -> bool {
  if (!battle_decided) {
    return false;
  }
  constexpr float k_decision_settle_seconds = 2.0F;
  if (elapsed < battle_decided_at + k_decision_settle_seconds) {
    return false;
  }
  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind == ArenaExpectationKind::BattleReachesDecision) {
      return true;
    }
  }
  return false;
}

auto ArenaScenarioRunner::Impl::side_result(const BattleSideState& side)
    -> ArenaBattleSideResult {
  ArenaBattleSideResult result;
  result.owner_id = side.owner_id;
  result.label = side.label;
  result.living_units = side.living_units;
  result.living_soldiers = side.living_soldiers;
  result.living_buildings = side.living_buildings;
  result.peak_units = side.peak_units;
  result.peak_soldiers = std::max(side.peak_soldiers, side.initial_soldiers);
  result.units_produced =
      std::max(0, static_cast<int>(side.seen_units.size()) - side.initial_units);
  result.peak_advance = side.peak_advance;
  result.final_advance = side.final_advance;
  result.eliminated_at = side.eliminated_at;
  result.strategy = side.strategy;
  result.posture = side.posture;
  result.ai_state = side.ai_state;
  result.wave_committed = side.wave_committed;
  result.wave_size = side.wave_size;
  result.buildings_constructed =
      std::max(0,
               static_cast<int>(side.seen_buildings.size()) -
                   static_cast<int>(side.initial_buildings.size()));
  result.peak_buildings = side.peak_buildings;
  result.peak_home_units = side.peak_home_units;
  result.peak_forward_units = side.peak_forward_units;
  result.mean_home_share =
      side.home_share_samples > 0
          ? static_cast<float>(side.home_share_sum /
                               static_cast<double>(side.home_share_samples))
          : 0.0F;
  {
    QStringList census;
    auto keys = side.building_census.keys();
    std::sort(keys.begin(), keys.end());
    for (auto const& key : keys) {
      census.push_back(
          QStringLiteral("%1x%2").arg(key).arg(side.building_census.value(key)));
    }
    result.building_census = census.join(QStringLiteral(","));
  }
  result.seconds_attacking = side.seconds_attacking;
  result.seconds_observed = side.seconds_observed;
  return result;
}

auto ArenaScenarioRunner::Impl::live_sides() const
    -> std::vector<ArenaBattleSideResult> {
  std::vector<ArenaBattleSideResult> sides;
  sides.reserve(battle_sides.size());
  for (auto const& side : battle_sides) {
    sides.push_back(side_result(side));
  }
  return sides;
}

void ArenaScenarioRunner::Impl::publish_battle_outcome() {
  if (battle_sides.empty()) {
    return;
  }
  report.battle.tracked = true;
  report.battle.decided = battle_decided;
  report.battle.decided_at_seconds = battle_decided_at;
  report.battle.sides.clear();
  for (auto const& side : battle_sides) {
    report.battle.sides.push_back(side_result(side));
    if (battle_decided && side.eliminated_at < 0.0F) {
      report.battle.victor_owner_id = side.owner_id;
      report.battle.victor_label = side.label;
    }
  }
}

auto ArenaScenarioRunner::Impl::windowed_peak_advance(
    const BattleSideState& side,
    const ArenaExpectation& expectation) -> std::optional<float> {
  float const start = expectation.start_seconds;
  float const end = expectation.end_seconds > 0.0F
                        ? expectation.end_seconds
                        : std::numeric_limits<float>::infinity();
  std::optional<float> peak;
  for (auto const& [time, advance] : side.advance_samples) {
    if (time < start || time > end) {
      continue;
    }
    peak = peak.has_value() ? std::max(*peak, advance) : advance;
  }
  return peak;
}

auto ArenaScenarioRunner::Impl::battle_side(const QString& label) const
    -> const BattleSideState* {
  for (auto const& side : battle_sides) {
    if (side.label == label) {
      return &side;
    }
  }
  return nullptr;
}

void ArenaScenarioRunner::Impl::observe_undead_zones() {
  if (scenario.undead_zones.empty()) {
    return;
  }
  auto const entities = world.collect_entities_with<Engine::Core::UnitComponent>();
  for (auto const& zone : scenario.undead_zones) {
    auto& state = undead_zone_states[zone.id];
    auto& seen = undead_zone_entities[zone.id];
    int alive = 0;
    for (auto* entity : entities) {
      auto const* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;

      if (unit == nullptr || unit->owner_id != zone.owner_id || unit->health <= 0 ||
          !Game::Units::is_troop_spawn(unit->spawn_type)) {
        continue;
      }
      ++alive;
      if (!seen.contains(entity->get_id())) {
        seen.insert(entity->get_id());
        ++state.spawned_total;
      }
    }
    if (alive > 0 && state.first_spawn_at < 0.0F) {
      state.first_spawn_at = elapsed;
    }
    state.alive = alive;
    state.peak_alive = std::max(state.peak_alive, alive);
    observe_zone_shrine(zone, state);
  }
}

void ArenaScenarioRunner::Impl::observe_zone_shrine(const Game::Map::UndeadZone& zone,
                                                    UndeadZoneObservation& state) {
  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  if (undead == nullptr) {
    return;
  }

  auto const anchor_id = undead->anchor_entity(zone.id);
  auto* anchor = anchor_id != 0 ? world.get_entity(anchor_id) : nullptr;
  auto const* unit = anchor != nullptr
                         ? anchor->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  bool const standing = unit != nullptr && unit->health > 0;

  if (standing) {
    state.shrine_seen = true;
  }
  state.shrine_standing = standing;
  if (state.shrine_seen && !standing) {
    state.shrine_destroyed = true;
  }
}

auto ArenaScenarioRunner::Impl::undead_zone_state(const QString& zone_id) const
    -> UndeadZoneObservation {
  auto const found = undead_zone_states.constFind(zone_id);
  return found == undead_zone_states.cend() ? UndeadZoneObservation{} : found.value();
}

void ArenaScenarioRunner::Impl::observe_range_rings() {
  auto* selection = &Game::Session::session_for(world).selection();
  if (selection == nullptr) {
    return;
  }
  const auto& selected = selection->get_selected_units();
  if (selected.empty()) {
    return;
  }

  Game::Systems::AttackRangeRingRequest request;
  request.world = &world;
  request.local_owner_id = 1;
  request.selection = selected;
  request.max_rings = Game::Systems::k_attack_range_max_rings;
  const auto rings = Game::Systems::collect_attack_range_rings(request);
  max_range_ring_count = std::max(max_range_ring_count, rings.size());

  for (auto const& group : scenario.groups) {
    auto const& group_ids = ids(group.name);
    for (auto const& ring : rings) {
      if (std::find(group_ids.begin(), group_ids.end(), ring.entity_id) ==
          group_ids.end()) {
        continue;
      }
      range_ring_max_radius[group.name] = ring.max_radius;
      range_ring_min_radius[group.name] = ring.min_radius;
    }
  }
}

void ArenaScenarioRunner::Impl::observe_commander_aura_state() {
  for (auto const& group : scenario.groups) {
    for (auto entity_id : ids(group.name)) {
      auto* entity = world.get_entity(entity_id);
      if (entity == nullptr) {
        continue;
      }
      if (auto const* commander =
              entity->get_component<Engine::Core::CommanderComponent>()) {
        if (commander->aura_ability_active) {
          commander_aura_active_seen[group.name] = true;
        } else if (commander_aura_active_seen.value(group.name, false) &&
                   commander->aura_ability_cooldown_remaining > 0.0F) {
          commander_aura_expired_seen[group.name] = true;
        }
      }
      if (auto const* buff =
              entity->get_component<Engine::Core::CommanderAuraBuffComponent>();
          buff != nullptr && buff->active) {
        commander_aura_buff_seen[group.name] = true;
      }
    }
  }
}

} // namespace Arena
