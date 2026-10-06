#include <QQuaternion>

#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

constexpr float k_default_idle_seconds = 1.25F;

constexpr float k_default_engagement_distance = 7.0F;

constexpr float k_spawn_settle_seconds = 0.10F;

constexpr float k_default_root_step = 0.8F;

constexpr float k_default_fall_up_y = 0.72F;

constexpr float k_default_foot_slide = 0.08F;

constexpr float k_planted_root_step = 0.02F;

constexpr float k_stride_fade_presence = 0.05F;

constexpr float k_planted_foot_height = 0.05F;

constexpr float k_default_hand_step = 0.90F;

constexpr float k_default_pelvis_step = 70.0F;

constexpr float k_default_attack_torso_sweep = 6.0F;

constexpr float k_locomotion_restart_presence = 0.5F;

constexpr float k_fastest_cycle_phase_rate = 2.5F;

constexpr float k_locomotion_phase_slack = 0.02F;

constexpr float k_attack_restart_phase = 0.35F;

constexpr float k_attack_restart_window_seconds = 0.4F;

constexpr float k_attack_resume_tolerance = 0.02F;

constexpr float k_default_body_pose_step = 0.30F;

constexpr float k_swing_body_pose_step = 0.45F;

constexpr float k_pose_snap_acceleration = 2.5F;

constexpr float k_pose_snap_margin = 0.08F;

auto wrapped_phase_delta(float now, float before) -> float {
  float delta = std::fmod(now - before, 1.0F);
  if (delta >= 0.5F) {
    delta -= 1.0F;
  } else if (delta < -0.5F) {
    delta += 1.0F;
  }
  return delta;
}

auto body_local_joints(const Render::Profiling::SoldierAnimationDebugSample& soldier)
    -> std::array<QVector3D, 4> {
  QQuaternion const to_body =
      QQuaternion::fromAxisAndAngle(0.0F, 1.0F, 0.0F, -soldier.root_yaw_degrees);
  auto local = [&](const QVector3D& joint) {
    return to_body.rotatedVector(joint - soldier.root_position);
  };
  return {local(soldier.hand_l_world),
          local(soldier.hand_r_world),
          local(soldier.foot_l_world),
          local(soldier.foot_r_world)};
}

auto soldier_key(Engine::Core::EntityID entity_id, int soldier_index) -> std::uint64_t {
  return (static_cast<std::uint64_t>(entity_id) << 32U) |
         static_cast<std::uint32_t>(std::max(0, soldier_index));
}

} // namespace

auto ArenaScenarioRunner::Impl::all_entities() const
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> result;
  for (auto const& group : scenario.groups) {
    auto const& group_ids = ids(group.name);
    result.insert(result.end(), group_ids.begin(), group_ids.end());
  }
  return result;
}

void ArenaScenarioRunner::Impl::track_elevation_leg(ElevationLegState& leg,
                                                    float elevation,
                                                    bool climbing,
                                                    float ceiling) {
  if (ceiling > 0.0F && elevation > ceiling) {
    leg.suspended = true;
    return;
  }
  if (leg.suspended) {
    leg.suspended = false;
    leg.extreme = elevation;
  }
  if (!leg.seeded) {
    leg.seeded = true;
    leg.extreme = elevation;
    return;
  }
  float const reversal = climbing ? leg.extreme - elevation : elevation - leg.extreme;
  if (reversal > leg.worst_reversal) {
    leg.worst_reversal = reversal;
    leg.worst_at = elapsed;
  }
  leg.extreme =
      climbing ? std::max(leg.extreme, elevation) : std::min(leg.extreme, elevation);
}

auto ArenaScenarioRunner::Impl::expectation_active(
    const ArenaExpectation& expectation) const -> bool {
  if (elapsed + 1.0e-5F < expectation.start_seconds) {
    return false;
  }
  return expectation.end_seconds <= 0.0F ||
         elapsed <= expectation.end_seconds + 1.0e-5F;
}

auto ArenaScenarioRunner::Impl::rpg_health_protection_active(const QString& group) const
    -> bool {
  bool has_protection = false;
  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind != ArenaExpectationKind::RpgHealthUnchanged ||
        expectation.group != group) {
      continue;
    }
    has_protection = true;
    if (expectation_active(expectation)) {
      return true;
    }
  }
  return !has_protection;
}

auto ArenaScenarioRunner::Impl::projectile_pair_key(
    Engine::Core::EntityID attacker_id,
    Engine::Core::EntityID target_id) const -> QString {
  auto const attacker = entity_groups.constFind(attacker_id);
  auto const target = entity_groups.constFind(target_id);
  if (attacker == entity_groups.cend() || target == entity_groups.cend()) {
    return {};
  }
  return attacker.value() + QChar(0x1f) + target.value();
}

auto ArenaScenarioRunner::Impl::projectile_pair_key(
    const QString& attacker_group, const QString& target_group) -> QString {
  return attacker_group + QChar(0x1f) + target_group;
}

void ArenaScenarioRunner::Impl::observe_projectiles() {
  auto const* system = world.get_system<Game::Systems::ProjectileSystem>();
  if (system == nullptr) {
    return;
  }
  for (auto const& projectile : system->projectiles()) {
    if (projectile == nullptr || !projectile->is_active() ||
        projectile->get_progress() < 0.0F) {
      continue;
    }
    auto const key =
        projectile_pair_key(projectile->get_attacker_id(), projectile->get_target_id());
    if (!key.isEmpty()) {
      projectile_flights[key] = true;
      if (Game::Systems::is_incendiary_projectile_kind(projectile->get_kind())) {
        flaming_projectile_flights[key] = true;
      } else {
        plain_projectile_flights[key] = true;
      }
    }
  }
  for (auto const& impact : system->impacts()) {
    if (observed_projectile_impacts.contains(impact.sequence)) {
      continue;
    }
    observed_projectile_impacts.insert(impact.sequence);
    auto const key = projectile_pair_key(impact.attacker_id, impact.target_id);
    if (!key.isEmpty() && impact.hit_target) {
      projectile_contacts[key] = true;
    }
    if (!key.isEmpty() && impact.hit_target && impact.damage_applied) {
      projectile_impacts[key] = true;
    }
  }
}

auto ArenaScenarioRunner::Impl::applies_to(const ArenaExpectation& expectation,
                                           const QString& group) const -> bool {
  return expectation.group.isEmpty() || expectation.group == group;
}

void ArenaScenarioRunner::Impl::observe_building_clearance(
    Engine::Core::EntityID entity_id, const QString& group) {
  if (building_overlap_report.contains(group)) {
    return;
  }
  const bool wanted = std::any_of(
      scenario.expectations.begin(),
      scenario.expectations.end(),
      [&](const ArenaExpectation& expectation) {
        return expectation.kind == ArenaExpectationKind::UnitsClearOfBuildings &&
               expectation_active(expectation) && applies_to(expectation, group);
      });
  if (!wanted) {
    return;
  }
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  if (transform == nullptr || unit == nullptr || unit->health <= 0) {
    return;
  }
  if (Game::Units::is_building_spawn(unit->spawn_type)) {
    return;
  }
  constexpr float k_body_radius = 0.35F;
  if (host.building_collision == nullptr) {
    return;
  }
  auto const& registry = *host.building_collision;
  const float unit_x = transform->position.x;
  const float unit_z = transform->position.z;
  const bool in_gateway =
      std::any_of(registry.navigation_passages().begin(),
                  registry.navigation_passages().end(),
                  [&](const Game::Systems::NavigationPassage& passage) {
                    return std::abs(unit_x - passage.center_x) <=
                               (passage.width * 0.5F) + k_body_radius &&
                           std::abs(unit_z - passage.center_z) <=
                               (passage.depth * 0.5F) + k_body_radius;
                  });
  if (!in_gateway &&
      registry.is_circle_overlapping_building(unit_x, unit_z, k_body_radius)) {
    building_overlap_report.insert(
        group,
        QStringLiteral("entity %1 stood inside a building at (%2, %3) after %4 s")
            .arg(entity_id)
            .arg(unit_x, 0, 'f', 1)
            .arg(unit_z, 0, 'f', 1)
            .arg(elapsed, 0, 'f', 1));
  }
}

auto ArenaScenarioRunner::Impl::tracks_narrow_layout(const QString& group) -> bool {
  if (!narrow_layout_groups_resolved) {
    narrow_layout_groups_resolved = true;
    for (auto const& expectation : scenario.expectations) {
      switch (expectation.kind) {
      case ArenaExpectationKind::NarrowLayoutEngaged:
      case ArenaExpectationKind::NarrowLayoutStaysWide:
      case ArenaExpectationKind::NarrowLayoutKeepsFiles:
      case ArenaExpectationKind::NarrowLayoutModeSettles:
      case ArenaExpectationKind::NarrowLayoutRestores:
        narrow_layout_groups.insert(expectation.group);
        break;
      default:
        break;
      }
    }
  }
  return narrow_layout_groups.contains(group);
}

void ArenaScenarioRunner::Impl::observe_narrow_layout(
    const QString& group, Engine::Core::EntityID entity_id) {
  if (!tracks_narrow_layout(group)) {
    return;
  }
  auto const* traversal =
      world.try_get<Engine::Core::UnitTraversalLayoutStateComponent>(entity_id);
  if (traversal == nullptr) {
    return;
  }
  auto& state = narrow_layout[group];
  if (!state.seeded) {
    state.seeded = true;
    state.narrowest_corridor = std::numeric_limits<float>::max();
    state.narrowest_files = traversal->normal_files;
    state.tightest_file_spacing = traversal->authored_file_spacing;
    state.previous_mode = traversal->mode;
  }
  state.formation_half_width =
      std::max(state.formation_half_width, traversal->desired_half_width);
  if (traversal->available_half_width > 0.0F) {
    state.narrowest_corridor =
        std::min(state.narrowest_corridor, traversal->available_half_width);
  }
  state.normal_files = std::max(state.normal_files, traversal->normal_files);
  if (traversal->mode != state.previous_mode) {
    ++state.mode_changes;
    state.previous_mode = traversal->mode;
  }
  state.active_at_end = traversal->active;
  state.files_at_end = traversal->target_files;

  float frontage = 0.0F;
  float front = 0.0F;
  float back = 0.0F;
  float reform_error = 0.0F;
  for (auto const& slot : traversal->slot_states) {
    if (!slot.alive) {
      continue;
    }
    frontage = std::max(frontage, std::abs(slot.current_local_x) * 2.0F);
    front = std::max(front, slot.current_local_z);
    back = std::min(back, slot.current_local_z);
    reform_error = std::max(reform_error,
                            std::hypot(slot.current_local_x - slot.target_local_x,
                                       slot.current_local_z - slot.target_local_z));
  }
  state.worst_reform_error = reform_error;
  if (!traversal->active) {
    return;
  }
  state.engaged = true;
  if (state.engaged_from < 0.0F) {
    state.engaged_from = elapsed;
  }
  state.engaged_until = elapsed;
  state.deepest_column = std::max(state.deepest_column, front - back);
  state.tightest_file_spacing =
      std::min(state.tightest_file_spacing,
               traversal->authored_file_spacing * traversal->lateral_scale);
  if (frontage > 0.0F) {
    state.narrowest_frontage = state.narrowest_frontage > 0.0F
                                   ? std::min(state.narrowest_frontage, frontage)
                                   : frontage;
  }
  if (traversal->target_files < state.narrowest_files) {
    state.narrowest_files = traversal->target_files;
    state.narrowest_mode = traversal->target_mode;
  }
}

namespace {

constexpr float k_motion_settle_seconds = 0.20F;

constexpr float k_motion_max_sample_gap = 0.10F;

constexpr float k_teleport_floor = 0.35F;

constexpr float k_teleport_speed_margin = 2.5F;

constexpr float k_charge_speed_scale = 1.6F;

constexpr float k_facing_snap_floor_degrees = 35.0F;

constexpr float k_facing_snap_rate = 720.0F;

constexpr float k_fast_rotation_rate = 480.0F;

constexpr int k_fast_rotation_frames = 6;

constexpr float k_jitter_window_seconds = 1.0F;

constexpr float k_facing_jitter_step = 2.0F;

constexpr std::size_t k_facing_jitter_reversals = 3U;

constexpr float k_position_jitter_step = 0.03F;

constexpr std::size_t k_position_jitter_reversals = 4U;

void keep_recent(std::vector<float>& times, float now) {
  times.erase(
      std::remove_if(times.begin(),
                     times.end(),
                     [now](float at) { return now - at > k_jitter_window_seconds; }),
      times.end());
}

auto signed_yaw_step(float now, float before) -> float {
  return std::fmod(now - before + 540.0F, 360.0F) - 180.0F;
}

} // namespace

void ArenaScenarioRunner::Impl::observe_motion_quality(Engine::Core::EntityID entity_id,
                                                       const QString& group) {
  bool const wanted = std::any_of(
      scenario.expectations.begin(),
      scenario.expectations.end(),
      [&](const ArenaExpectation& expectation) {
        return expectation.kind == ArenaExpectationKind::EntityMotionIsSmooth &&
               expectation_active(expectation) && applies_to(expectation, group);
      });
  if (!wanted) {
    return;
  }
  auto* entity = world.get_entity(entity_id);
  auto const* transform =
      entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto const* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  auto& state = motion_states[entity_id];
  bool const alive = transform != nullptr && unit != nullptr && unit->health > 0 &&
                     !entity->has_component<Engine::Core::DeathAnimationComponent>();
  if (!alive) {
    state = {};
    return;
  }
  QVector3D const position(transform->position.x, 0.0F, transform->position.z);
  float const yaw = transform->rotation.y;
  float const dt = elapsed - state.observed_at;
  bool const comparable = state.initialized && dt > 0.0F &&
                          dt <= k_motion_max_sample_gap &&
                          elapsed >= k_motion_settle_seconds;
  if (comparable) {
    QVector3D const step = position - state.position;
    float const distance = step.length();
    float speed = unit->speed * k_charge_speed_scale;
    if (auto const* movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      speed = std::max(speed, std::hypot(movement->get_vx(), movement->get_vz()));
    }
    float const allowed_step =
        std::max(k_teleport_floor, speed * dt * k_teleport_speed_margin);
    if (distance > allowed_step) {
      add_issue(QStringLiteral("entity_teleport"),
                QStringLiteral("%1 entity %2 jumped %3 m in one frame (allowed %4 m)")
                    .arg(group)
                    .arg(entity_id)
                    .arg(distance, 0, 'f', 2)
                    .arg(allowed_step, 0, 'f', 2),
                entity_id);
    }

    auto const* formation =
        entity->get_component<Engine::Core::FormationPresentationComponent>();
    bool const drawn_as_soldiers =
        formation != nullptr && formation->soldiers.size() > 1U;
    float const yaw_step = drawn_as_soldiers ? 0.0F : signed_yaw_step(yaw, state.yaw);
    float const snap_limit =
        std::max(k_facing_snap_floor_degrees, k_facing_snap_rate * dt);
    if (std::abs(yaw_step) > snap_limit) {
      add_issue(QStringLiteral("facing_snap"),
                QStringLiteral("%1 entity %2 turned %3 degrees in one frame")
                    .arg(group)
                    .arg(entity_id)
                    .arg(yaw_step, 0, 'f', 1),
                entity_id);
    }
    state.fast_rotation_frames = std::abs(yaw_step) / dt > k_fast_rotation_rate
                                     ? state.fast_rotation_frames + 1
                                     : 0;
    if (state.fast_rotation_frames >= k_fast_rotation_frames) {
      add_issue(QStringLiteral("fast_rotation"),
                QStringLiteral("%1 entity %2 spun faster than %3 degrees/s for %4 "
                               "frames")
                    .arg(group)
                    .arg(entity_id)
                    .arg(k_fast_rotation_rate, 0, 'f', 0)
                    .arg(state.fast_rotation_frames),
                entity_id);
    }

    keep_recent(state.facing_reversals, elapsed);
    if (std::abs(yaw_step) > k_facing_jitter_step &&
        std::abs(state.last_yaw_step) > k_facing_jitter_step &&
        (yaw_step > 0.0F) != (state.last_yaw_step > 0.0F)) {
      state.facing_reversals.push_back(elapsed);
    }
    if (state.facing_reversals.size() >= k_facing_jitter_reversals) {
      add_issue(QStringLiteral("facing_jitter"),
                QStringLiteral("%1 entity %2 swung its facing back and forth %3 times "
                               "within a second")
                    .arg(group)
                    .arg(entity_id)
                    .arg(state.facing_reversals.size()),
                entity_id);
    }

    keep_recent(state.position_reversals, elapsed);
    float const last_distance = state.last_step.length();
    if (distance > k_position_jitter_step && last_distance > k_position_jitter_step &&
        QVector3D::dotProduct(step, state.last_step) <
            -0.5F * distance * last_distance) {
      state.position_reversals.push_back(elapsed);
    }
    if (state.position_reversals.size() >= k_position_jitter_reversals) {
      add_issue(QStringLiteral("position_jitter"),
                QStringLiteral("%1 entity %2 moved back and forth %3 times within a "
                               "second")
                    .arg(group)
                    .arg(entity_id)
                    .arg(state.position_reversals.size()),
                entity_id);
    }
    state.last_step = step;
    state.last_yaw_step = yaw_step;
  } else {
    state.last_step = {};
    state.last_yaw_step = 0.0F;
    state.fast_rotation_frames = 0;
  }
  state.position = position;
  state.yaw = yaw;
  state.observed_at = elapsed;
  state.initialized = true;
}

void ArenaScenarioRunner::Impl::observe_entity(Engine::Core::EntityID entity_id,
                                               const QString& group,
                                               TraceFrame& frame) {
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  if (transform == nullptr || unit == nullptr) {
    return;
  }
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return;
  }
  if (auto const* gate = world.try_get<Engine::Core::GateComponent>(entity_id)) {
    gate_seen[group] = true;
    if (gate->open_amount >= Engine::Core::GateComponent::k_passable_open_amount) {
      gate_opened_seen[group] = true;
    }
  }
  if (auto const* tower = world.try_get<Engine::Core::SiegeTowerComponent>(entity_id);
      tower != nullptr &&
      tower->state == Engine::Core::SiegeTowerComponent::State::Docked) {
    tower_docked_seen[group] = true;
  }
  if (auto const* rpg = world.try_get<Engine::Core::RpgHealthComponent>(entity_id);
      rpg != nullptr && rpg->active) {
    auto const* rpg_unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
    int const health = rpg_unit != nullptr ? rpg_unit->health : 0;
    if (!initial_rpg_health_by_group.contains(group)) {
      initial_rpg_health_by_group[group] = health;
      minimum_rpg_health_by_group[group] = health;
    } else if (rpg_health_protection_active(group)) {

      minimum_rpg_health_by_group[group] =
          std::min(minimum_rpg_health_by_group.value(group), health);
    }
    if (rpg->dodge_grace_remaining > 0.0F) {
      rpg_dodge_window_seen[group] = true;
    }
  }
  if (auto const* targets =
          world.try_get<Engine::Core::RpgCommanderTargetComponent>(entity_id);
      targets != nullptr && targets->aim_candidate_in_range &&
      targets->aim_candidate_id != 0) {
    auto* target = world.get_entity(targets->aim_candidate_id);
    bool const exact_slot_required =
        target != nullptr &&
        Game::Systems::FormationCombat::has_formation_slots(*target);
    bool const has_exact_slot =
        targets->aim_candidate_soldier_slot !=
        Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    if (target != nullptr && (!exact_slot_required || has_exact_slot) &&
        Game::Systems::RpgCombat::resolve_soldier_target(
            *target, targets->aim_candidate_soldier_slot)
            .has_value()) {
      exact_rpg_target_seen[group] = true;
    }
  }
  if (auto const* contacts =
          world.try_get<Engine::Core::RpgContactPresentationComponent>(entity_id)) {
    for (auto const& contact : contacts->entries) {
      switch (contact.outcome) {
      case Engine::Core::RpgContactOutcome::Damage:
        rpg_damage_contact_seen[group] = true;
        break;
      case Engine::Core::RpgContactOutcome::Block:
      case Engine::Core::RpgContactOutcome::PerfectGuard:
        rpg_block_contact_seen[group] = true;
        break;
      case Engine::Core::RpgContactOutcome::Dodge:
        rpg_dodge_contact_seen[group] = true;
        break;
      }
    }
  }
  if (auto const* structure_damage =
          world.try_get<Engine::Core::StructureDamagePresentationComponent>(entity_id);
      structure_damage != nullptr && !structure_damage->impacts.empty()) {
    structure_damage_cues[group] = true;
  }
  if (Game::Systems::Combat::structure_fire_intensity(*entity) > 0.0F) {
    structure_fires[group] = true;
  }
  if (Engine::Core::is_collapsing_structure(*entity)) {
    auto const* renderable =
        world.try_get<Engine::Core::RenderableComponent>(entity_id);
    if (renderable != nullptr && renderable->visible) {
      structure_collapses[group] = true;
    }
  }
  if (auto const* repair =
          world.try_get<Engine::Core::StructureRepairPresentationComponent>(entity_id);
      repair != nullptr && repair->scaffold >= 0.99F) {
    structure_repairs[group] = true;
  }
  if (auto const* dismantle =
          world.try_get<Engine::Core::DismantleSiteComponent>(entity_id);
      dismantle != nullptr && dismantle->progress >= 0.5F) {
    structure_dismantles[group] = true;
  }
  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind != ArenaExpectationKind::StructureFacadeContactObserved ||
        !expectation_active(expectation) || expectation.group != group) {
      continue;
    }
    for (auto target_id : ids(expectation.target_group)) {
      auto* structure = world.get_entity(target_id);
      if (structure != nullptr &&
          structure->has_component<Engine::Core::BuildingComponent>() &&
          Game::Systems::Combat::structure_melee_contact_active(
              *entity,
              *structure,
              Engine::Core::AttackComponent::k_melee_contact_range_grace)) {
        structure_facade_contacts[projectile_pair_key(expectation.group,
                                                      expectation.target_group)] = true;
        break;
      }
    }
  }
  if (Game::Systems::DefensiveUnitLayoutService::is_formed(*entity)) {
    defensive_layout_locked[group] = true;
  }
  if (auto const* movement_facts =
          world.try_get<Engine::Core::MovementFactsComponent>(entity_id)) {
    auto const& stall = movement_facts->progress.stall;
    auto& observation = stall_observations[group];
    float const going_nowhere =
        std::max(stall.stalled_seconds, stall.no_closer_seconds);
    if (going_nowhere > observation.worst_stalled_seconds) {
      observation.worst_stalled_seconds = going_nowhere;
      observation.worst_stalled_at = elapsed;
      observation.worst_state = QString::fromLatin1(
          Engine::Core::movement_state_name(movement_facts->progress.state));
      observation.has_objective = stall.objective_valid;
      observation.objective_x = stall.objective_x;
      observation.objective_z = stall.objective_z;
    }
    observation.recovery_attempts =
        std::max(observation.recovery_attempts, stall.recovery_attempts);
    observation.repaths =
        std::max(observation.repaths, movement_facts->progress.repath_count);
    observation.abandons = std::max(observation.abandons, stall.abandon_count);
    observation.recovery_seen = observation.recovery_seen ||
                                stall.rung != Engine::Core::MovementRecoveryRung::None;
  }
  QVector3D const position = vector_from_transform(*transform);
  if (!initial_elevation.contains(group)) {
    initial_elevation[group] = position.y();
    maximum_elevation[group] = position.y();
  } else {
    maximum_elevation[group] = std::max(maximum_elevation.value(group), position.y());
  }
  if (host.terrain != nullptr &&
      host.terrain->is_on_bridge(position.x(), position.z())) {
    bridge_traversal_seen[group] = true;
  }
  for (auto const& expectation : scenario.expectations) {
    if (expectation.group != group || !expectation_active(expectation)) {
      continue;
    }
    switch (expectation.kind) {
    case ArenaExpectationKind::ElevationClimbIsMonotonic:
      track_elevation_leg(
          elevation_climb_legs[group], position.y(), true, expectation.distance);
      break;
    case ArenaExpectationKind::ElevationDescentIsMonotonic:
      track_elevation_leg(
          elevation_descent_legs[group], position.y(), false, expectation.distance);
      break;
    case ArenaExpectationKind::ElevationHeldAbove: {
      auto& floor = elevation_floors[group];
      if (!floor.seeded || position.y() < floor.lowest) {
        floor.seeded = true;
        floor.lowest = position.y();
        floor.lowest_at = elapsed;
        floor.lowest_where = position;
      }
      break;
    }
    case ArenaExpectationKind::UnitsStayOnWalkableGround: {
      if (Game::Systems::NavGrid::is_world_position_walkable(position)) {
        break;
      }
      auto& state = off_walkable_ground[group];
      state.samples += 1;
      if (state.samples == 1) {
        state.worst = position;
        state.worst_at = elapsed;
      }
      break;
    }
    case ArenaExpectationKind::SoldiersStayOnWalkableGround: {
      auto const* presentation =
          world.try_get<Engine::Core::FormationPresentationComponent>(entity_id);
      if (presentation == nullptr) {
        break;
      }
      float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
      float const sin_yaw = std::sin(yaw);
      float const cos_yaw = std::cos(yaw);
      for (auto const& soldier : presentation->soldiers) {
        if (!soldier.alive) {
          continue;
        }
        QVector3D const world(
            position.x() + (cos_yaw * soldier.local_x) + (sin_yaw * soldier.local_z),
            0.0F,
            position.z() - (sin_yaw * soldier.local_x) + (cos_yaw * soldier.local_z));
        auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
        if (pathfinder == nullptr) {
          continue;
        }
        auto const cell = pathfinder->world_to_grid(world.x(), world.z());
        if (pathfinder->is_terrain_walkable(cell.x, cell.y)) {
          continue;
        }
        auto& state = off_walkable_soldiers[group];
        state.samples += 1;
        if (state.samples == 1) {
          state.worst = world;
          state.worst_at = elapsed;
        }
      }
      break;
    }
    default:
      break;
    }
  }
  auto const* target = world.try_get<Engine::Core::AttackTargetComponent>(entity_id);
  auto const* motion =
      world.try_get<Engine::Core::MotionPresentationComponent>(entity_id);
  auto const* formation_contact =
      world.try_get<Engine::Core::FormationContactComponent>(entity_id);
  auto const* attack = world.try_get<Engine::Core::AttackComponent>(entity_id);
  auto const* movement = world.try_get<Engine::Core::MovementComponent>(entity_id);
  auto const* mounted_charge =
      world.try_get<Engine::Core::MountedChargeComponent>(entity_id);
  auto const* combat_action =
      world.try_get<Engine::Core::RpgCommanderActionComponent>(entity_id);
  if (auto const* casualties =
          world.try_get<Engine::Core::SoldierCasualtyAnimationComponent>(entity_id);
      casualties != nullptr &&
      std::any_of(casualties->entries.begin(),
                  casualties->entries.end(),
                  [](auto const& entry) { return entry.launched; })) {
    launched_casualties[group] = true;
  }
  auto const* builder =
      world.try_get<Engine::Core::BuilderProductionComponent>(entity_id);
  if (builder != nullptr && builder->construction_complete &&
      !latched_builder_completions.contains(entity_id)) {
    if (Game::Systems::is_gather_builder_product(builder->product_type)) {
      completed_harvest_by_owner[unit->owner_id]++;
    }
    latched_builder_completions.insert(entity_id);
  } else if (builder != nullptr && !builder->construction_complete) {
    latched_builder_completions.remove(entity_id);
  }
  bool const mounted_charge_impact =
      combat_action != nullptr &&
      combat_action->combat_action_id ==
          static_cast<std::uint8_t>(
              Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  if (mounted_charge_impact) {
    charge_impacts[group] = true;
  }
  if (charge_impacts.value(group, false) && attack != nullptr &&
      attack->in_melee_lock && !mounted_charge_impact) {
    melee_locks_after_charge[group] = true;
  }
  bool const combat_indicator_submitted =
      Render::Profiling::CombatAnimationDiagnostics::instance()
          .mode_indicator_submitted(entity_id);
  QString combat_mode = QStringLiteral("none");
  if (attack != nullptr) {
    combat_mode =
        attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged
            ? QStringLiteral("ranged")
            : (attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee
                   ? QStringLiteral("melee")
                   : QStringLiteral("auto"));
  }
  QString motion_name = QStringLiteral("idle");
  if (motion != nullptr) {
    motion_name = motion->is_run_state()
                      ? QStringLiteral("run")
                      : (motion->is_walk_state() ? QStringLiteral("walk")
                                                 : QStringLiteral("idle"));
  }
  if (formation_contact != nullptr && formation_contact->in_contact) {
    auto const current = minimum_formation_surface_gap.find(group);
    if (current == minimum_formation_surface_gap.end() ||
        formation_contact->surface_gap < current.value()) {
      minimum_formation_surface_gap[group] = formation_contact->surface_gap;
    }
  }
  frame.units.push_back(
      {entity_id,
       group,
       position,
       unit->health,
       target != nullptr ? target->target_id : 0U,
       motion_name,
       combat_mode,
       mounted_charge != nullptr ? static_cast<int>(mounted_charge->state) : -1,
       mounted_charge != nullptr ? static_cast<int>(mounted_charge->last_cancel_reason)
                                 : -1,
       combat_action != nullptr ? combat_action->combat_action_id : 0,
       attack != nullptr && attack->in_melee_lock,
       attack != nullptr ? attack->melee_lock_target_id : 0U,
       combat_indicator_submitted,
       transform->rotation.y,
       movement != nullptr && movement->get_has_target(),
       movement != nullptr ? movement->get_vx() : 0.0F,
       movement != nullptr ? movement->get_vz() : 0.0F,
       movement != nullptr ? movement->get_goal_x() : 0.0F,
       movement != nullptr ? movement->get_goal_y() : 0.0F,
       formation_contact != nullptr && formation_contact->in_contact,
       formation_contact != nullptr ? formation_contact->surface_gap : 0.0F,
       formation_contact != nullptr ? formation_contact->engaged_soldier_indices
                                    : std::vector<std::uint16_t>{},
       formation_contact != nullptr
           ? formation_contact->engagement_pairs
           : std::vector<Engine::Core::FormationEngagementPair>{},
       builder != nullptr ? QString::fromStdString(builder->product_type) : QString{},
       builder != nullptr && builder->has_construction_site,
       builder != nullptr && builder->in_progress,
       builder != nullptr ? builder->time_remaining : 0.0F,
       [&]() {
         auto const* commander =
             world.try_get<Engine::Core::CommanderComponent>(entity_id);
         return commander != nullptr && commander->aura_ability_active;
       }(),
       [&]() {
         auto const* buff =
             world.try_get<Engine::Core::CommanderAuraBuffComponent>(entity_id);
         return buff != nullptr && buff->active;
       }(),
       [&]() {
         auto const* rpg = world.try_get<Engine::Core::RpgHealthComponent>(entity_id);
         auto const* rpg_unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
         return rpg != nullptr && rpg->active && rpg_unit != nullptr ? rpg_unit->health
                                                                     : -1;
       }(),
       [&]() {
         auto const* guard =
             world.try_get<Engine::Core::CommanderGuardComponent>(entity_id);
         return guard != nullptr && guard->active;
       }(),
       [&]() {
         auto const* rpg = world.try_get<Engine::Core::RpgHealthComponent>(entity_id);
         return rpg != nullptr && rpg->active && rpg->dodge_grace_remaining > 0.0F;
       }(),
       [&]() {
         auto const* targets =
             world.try_get<Engine::Core::RpgCommanderTargetComponent>(entity_id);
         return targets != nullptr ? targets->aim_candidate_id
                                   : Engine::Core::EntityID{0};
       }(),
       [&]() {
         auto const* targets =
             world.try_get<Engine::Core::RpgCommanderTargetComponent>(entity_id);
         if (targets == nullptr ||
             targets->aim_candidate_soldier_slot ==
                 Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot) {
           return -1;
         }
         return static_cast<int>(targets->aim_candidate_soldier_slot);
       }(),
       [&]() {
         auto const* action =
             world.try_get<Engine::Core::RpgCommanderActionComponent>(entity_id);
         return action != nullptr ? static_cast<int>(action->phase) : 0;
       }(),
       [&]() {
         auto const* action =
             world.try_get<Engine::Core::RpgCommanderActionComponent>(entity_id);
         return action != nullptr ? action->normalized_action_time : 0.0F;
       }()});

  {
    auto& traced = frame.units.back();
    auto const& engagement_trace = Game::Systems::Combat::EngagementTrace::instance();
    if (auto const* record = engagement_trace.find(entity_id)) {
      traced.engagement_candidate_id = record->candidate_id;
      traced.engagement_target_id = record->target_id;
      traced.engagement_range = record->acquisition_range;
      auto const reason =
          Game::Systems::Combat::engagement_outcome_key(record->outcome);
      traced.engagement_reason =
          QString::fromUtf8(reason.data(), static_cast<qsizetype>(reason.size()));
    } else {
      traced.engagement_reason = QStringLiteral("not_evaluated");
    }
    auto const command_source =
        Game::Systems::Combat::command_source_of(world.get_entity(entity_id));
    auto const source = Game::Systems::Combat::command_source_key(command_source);
    traced.command_source =
        QString::fromUtf8(source.data(), static_cast<qsizetype>(source.size()));

    bool const holds_automatic_target =
        traced.target_id != 0 &&
        command_source == Game::Systems::Combat::CommandSource::Auto;
    if (holds_automatic_target) {
      auto_engaged_entities.insert(entity_id);
      for (auto const& expectation : scenario.expectations) {
        bool const windowed =
            expectation.kind == ArenaExpectationKind::EngagementReleasedByOrder ||
            expectation.kind == ArenaExpectationKind::NoAutoEngagementObserved;
        if (!windowed || !expectation_active(expectation) ||
            !applies_to(expectation, group) ||
            reported_engagement_windows.contains(entity_id)) {
          continue;
        }
        reported_engagement_windows.insert(entity_id);
        if (expectation.kind == ArenaExpectationKind::EngagementReleasedByOrder) {
          add_issue(QStringLiteral("order_did_not_release_engagement"),
                    QStringLiteral("%1 entity %2 still held automatic target %3 "
                                   "after an explicit order")
                        .arg(group)
                        .arg(entity_id)
                        .arg(traced.target_id),
                    entity_id);
        } else {
          add_issue(QStringLiteral("unexpected_auto_engagement"),
                    QStringLiteral("%1 entity %2 picked its own fight with %3 when "
                                   "it should not have")
                        .arg(group)
                        .arg(entity_id)
                        .arg(traced.target_id),
                    entity_id);
        }
      }
    }
  }

  auto& previous = entity_states[entity_id];

  if (attack != nullptr && attack->in_melee_lock && !combat_indicator_submitted) {
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind == ArenaExpectationKind::CombatIndicatorIsContinuous &&
          expectation_active(expectation) && applies_to(expectation, group)) {
        add_issue(QStringLiteral("missing_combat_indicator"),
                  QStringLiteral("%1 entity %2 had a melee lock but submitted no "
                                 "fight-mode indicator in the rendered frame")
                      .arg(group)
                      .arg(entity_id),
                  entity_id);
      }
    }
  }
  float const sample_dt = previous.initialized ? elapsed - previous.observed_at : 0.0F;
  if (previous.initialized && sample_dt > 0.0F) {
    float const step = horizontal_distance(position, previous.position);
    for (auto const& expectation : scenario.expectations) {
      if (!expectation_active(expectation) || !applies_to(expectation, group)) {
        continue;
      }
      if (expectation.kind == ArenaExpectationKind::MovementIsContinuous) {

        float const multiplier =
            expectation.threshold > 0.0F ? expectation.threshold : 2.75F;
        float const allowed = std::max(0.25F, unit->speed * sample_dt * multiplier);
        if (step > allowed) {
          add_issue(QStringLiteral("movement_discontinuity"),
                    QStringLiteral("%1 entity %2 moved %3 m in one rendered frame "
                                   "(allowed %4 m)")
                        .arg(group)
                        .arg(entity_id)
                        .arg(step, 0, 'f', 2)
                        .arg(allowed, 0, 'f', 2),
                    entity_id);
        }
      }
      if (expectation.kind == ArenaExpectationKind::FormationEngagementIsStable &&
          previous.melee_lock && entity_alive(previous.melee_lock_target_id)) {
        Engine::Core::EntityID const current_lock_target =
            attack != nullptr && attack->in_melee_lock ? attack->melee_lock_target_id
                                                       : 0U;
        if (current_lock_target != previous.melee_lock_target_id) {
          add_issue(QStringLiteral("melee_engagement_restarted"),
                    QStringLiteral("%1 entity %2 released or changed a living "
                                   "melee opponent during the engagement")
                        .arg(group)
                        .arg(entity_id),
                    entity_id);
        }
        bool const locomoting = motion != nullptr && motion->has_locomotion();
        bool const navigation_active =
            movement != nullptr &&
            (movement->get_has_target() || std::abs(movement->get_vx()) > 0.001F ||
             std::abs(movement->get_vz()) > 0.001F);
        if (locomoting || navigation_active || step > 0.005F) {
          add_issue(QStringLiteral("movement_during_melee_engagement"),
                    QStringLiteral("%1 entity %2 walked while its melee opponent "
                                   "was still alive")
                        .arg(group)
                        .arg(entity_id),
                    entity_id);
        }
        float const yaw_delta = std::abs(
            std::fmod(transform->rotation.y - previous.yaw + 540.0F, 360.0F) - 180.0F);
        if (yaw_delta > 0.05F) {
          add_issue(QStringLiteral("rotation_during_melee_engagement"),
                    QStringLiteral("%1 entity %2 changed facing by %3 degrees "
                                   "while melee-locked")
                        .arg(group)
                        .arg(entity_id)
                        .arg(yaw_delta, 0, 'f', 2),
                    entity_id);
        }
      }
    }
    if (unit->health < previous.health) {
      damage_seen[group] = true;
    }
  }
  previous = {position,
              elapsed,
              unit->health,
              transform->rotation.y,
              attack != nullptr ? attack->melee_lock_target_id : 0U,
              attack != nullptr && attack->in_melee_lock,
              true};

  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind != ArenaExpectationKind::FormationEngagementIsStable ||
        !expectation_active(expectation) || !applies_to(expectation, group) ||
        formation_contact == nullptr || !formation_contact->in_contact ||
        attack == nullptr || !attack->in_melee_lock) {
      continue;
    }
    auto const* formation =
        world.try_get<Engine::Core::FormationPresentationComponent>(entity_id);
    std::size_t const living =
        formation != nullptr ? static_cast<std::size_t>(std::count_if(
                                   formation->soldiers.begin(),
                                   formation->soldiers.end(),
                                   [](auto const& soldier) { return soldier.alive; }))
                             : 0U;
    QSet<std::uint16_t> paired_slots;
    for (auto const& pair : formation_contact->engagement_pairs) {
      paired_slots.insert(pair.attacker_slot);
    }
    if (living > 0U && paired_slots.size() != static_cast<qsizetype>(living)) {
      add_issue(QStringLiteral("incomplete_formation_engagement"),
                QStringLiteral("%1 entity %2 engaged %3 of %4 living soldiers")
                    .arg(group)
                    .arg(entity_id)
                    .arg(paired_slots.size())
                    .arg(living),
                entity_id);
    }
    auto* lock_target = world.get_entity(attack->melee_lock_target_id);
    if (lock_target != nullptr) {
      auto const geometry =
          Game::Systems::FormationCombat::contact_geometry(*entity, *lock_target);
      if (!previous.melee_lock && geometry.formation_overlap_required &&
          geometry.center_distance > geometry.engagement_center_distance + 0.01F) {
        add_issue(QStringLiteral("insufficient_formation_overlap"),
                  QStringLiteral("%1 entity %2 locked at center distance %3 m "
                                 "instead of the %4 m overlap contract")
                      .arg(group)
                      .arg(entity_id)
                      .arg(geometry.center_distance, 0, 'f', 3)
                      .arg(geometry.engagement_center_distance, 0, 'f', 3),
                  entity_id);
      }
    }
  }

  if (auto response = responses.find(entity_id);
      response != responses.end() && !response->observed) {
    bool const moved =
        horizontal_distance(position, response->initial_position) > 0.03F;
    bool const turned =
        std::abs(
            std::fmod(transform->rotation.y - response->initial_yaw + 540.0F, 360.0F) -
            180.0F) > 5.0F;
    bool const visually_active = motion != nullptr && motion->has_locomotion();
    auto const* combat = world.try_get<Engine::Core::CombatStateComponent>(entity_id);
    bool const combat_active =
        combat != nullptr &&
        combat->animation_state != Engine::Core::CombatAnimationState::Idle;
    auto const* hold = world.try_get<Engine::Core::HoldModeComponent>(entity_id);
    bool const stance_exit_accepted =
        response->command == QStringLiteral("ReleaseReserve") && hold != nullptr &&
        !hold->active;
    response->observed =
        moved || turned || visually_active || combat_active || stance_exit_accepted;
    if (!response->observed && elapsed > response->deadline && !response->reported) {
      response->reported = true;
      add_issue(QStringLiteral("command_response_timeout"),
                QStringLiteral("%1 entity %2 did not visibly respond to %3 within "
                               "%4 s")
                    .arg(group)
                    .arg(entity_id)
                    .arg(response->command)
                    .arg(response->deadline - response->issued_at, 0, 'f', 2),
                entity_id);
    }
  }

  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind != ArenaExpectationKind::NoEligibleTroopIdleDuringCombat ||
        !expectation_active(expectation) || !applies_to(expectation, group) ||
        expectation.target_group.isEmpty() || unit->health <= 0) {
      continue;
    }
    float const distance = groups_distance(group, expectation.target_group);
    float const eligible_distance = expectation.distance > 0.0F
                                        ? expectation.distance
                                        : k_default_engagement_distance;
    bool const active_order = target != nullptr && target->target_id != 0U;
    bool const active_motion = motion != nullptr && motion->has_locomotion();
    bool const eligible_idle =
        distance <= eligible_distance && !active_order && !active_motion;
    if (!eligible_idle) {
      idle_since.remove(entity_id);
      continue;
    }
    if (!idle_since.contains(entity_id)) {
      idle_since.insert(entity_id, elapsed);
    }
    float const allowed_idle =
        expectation.threshold > 0.0F ? expectation.threshold : k_default_idle_seconds;
    if (elapsed - idle_since.value(entity_id) > allowed_idle) {
      add_issue(QStringLiteral("eligible_troop_idle"),
                QStringLiteral("%1 entity %2 stayed idle for %3 s while an eligible "
                               "enemy was %4 m away")
                    .arg(group)
                    .arg(entity_id)
                    .arg(elapsed - idle_since.value(entity_id), 0, 'f', 2)
                    .arg(distance, 0, 'f', 2),
                entity_id);
    }
  }

  if (motion != nullptr && motion->has_locomotion()) {
    useful_bot_action[group] = true;
  }
  if (target != nullptr && target->target_id != 0U) {
    useful_bot_action[group] = true;
  }
}

void ArenaScenarioRunner::Impl::observe_bridge_centerline_alignment(
    const QString& group) {
  QVector3D centroid;
  int living = 0;
  for (auto entity_id : ids(group)) {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform == nullptr || !entity_alive(entity_id)) {
      continue;
    }
    centroid += vector_from_transform(*transform);
    ++living;
  }
  if (living == 0) {
    return;
  }
  centroid /= static_cast<float>(living);

  auto const* height_map =
      host.terrain != nullptr ? host.terrain->get_height_map() : nullptr;
  if (height_map == nullptr) {
    return;
  }
  for (auto const& bridge : height_map->get_bridges()) {
    QVector3D direction = bridge.end - bridge.start;
    direction.setY(0.0F);
    float const length = direction.length();
    if (length < 0.01F) {
      continue;
    }
    direction /= length;
    QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
    QVector3D offset = centroid - bridge.start;
    offset.setY(0.0F);
    float const along = QVector3D::dotProduct(offset, direction);
    float const lateral = std::abs(QVector3D::dotProduct(offset, perpendicular));
    if (along < 0.0F || along > length || lateral > bridge.width * 0.5F) {
      continue;
    }

    float const midpoint_distance = std::abs(along - length * 0.5F);
    auto& observation = bridge_alignment[group];
    if (!observation.sampled || midpoint_distance < observation.midpoint_distance) {
      observation.sampled = true;
      observation.midpoint_distance = midpoint_distance;
      observation.lateral_offset = lateral;
    }
  }
}

void ArenaScenarioRunner::Impl::observe_rpg_locomotion_presentation(
    const TraceFrame& frame) {
  if (scenario.rpg_commander_group.isEmpty()) {
    return;
  }
  const QString& group = scenario.rpg_commander_group;
  for (auto const& unit : frame.units) {
    if (unit.group != group) {
      continue;
    }
    if (unit.motion == QStringLiteral("walk")) {
      rpg_walk_seen[group] = true;
    } else if (unit.motion == QStringLiteral("run")) {
      rpg_run_seen[group] = true;
    }
    for (auto const& soldier : frame.soldiers) {
      if (soldier.entity_id != unit.entity_id || soldier.culled) {
        continue;
      }

      constexpr float k_strike_sync_grace_seconds = 0.05F;
      const bool striking =
          unit.rpg_action_phase ==
          static_cast<int>(Engine::Core::RpgCommanderActionPhase::Strike);

      const bool strike_visual = soldier.visual == QStringLiteral("Attack") ||
                                 soldier.visual == QStringLiteral("Dying") ||
                                 soldier.visual == QStringLiteral("Dead");
      if (!striking || strike_visual) {
        rpg_strike_mismatch_since.remove(group);
      } else {
        if (!rpg_strike_mismatch_since.contains(group)) {
          rpg_strike_mismatch_since[group] = frame.time_seconds;
        }
        float const lagged =
            frame.time_seconds - rpg_strike_mismatch_since.value(group);
        if (lagged > k_strike_sync_grace_seconds &&
            !rpg_strike_mismatch.contains(group)) {
          rpg_strike_mismatch[group] =
              QStringLiteral("simulation resolved a strike for %1 s while the "
                             "renderer still showed %2 at %3 s")
                  .arg(lagged, 0, 'f', 3)
                  .arg(soldier.visual)
                  .arg(frame.time_seconds, 0, 'f', 2);
        }
      }

      const bool visual_moving = soldier.visual == QStringLiteral("Walk") ||
                                 soldier.visual == QStringLiteral("Run");
      const bool visual_running = soldier.visual == QStringLiteral("Run");
      const bool combat_visual = soldier.visual == QStringLiteral("Attack") ||
                                 soldier.visual == QStringLiteral("HitReaction") ||
                                 soldier.visual == QStringLiteral("Dying") ||
                                 soldier.visual == QStringLiteral("Dead");
      if (combat_visual || rpg_locomotion_mismatch.contains(group)) {
        continue;
      }
      if (unit.motion == QStringLiteral("run") && !visual_running) {
        rpg_locomotion_mismatch[group] =
            QStringLiteral("simulation reported run at %1 s while the renderer showed "
                           "%2")
                .arg(frame.time_seconds, 0, 'f', 2)
                .arg(soldier.visual);
      } else if (unit.motion == QStringLiteral("walk") && !visual_moving) {
        rpg_locomotion_mismatch[group] =
            QStringLiteral("simulation reported walk at %1 s while the renderer showed "
                           "%2")
                .arg(frame.time_seconds, 0, 'f', 2)
                .arg(soldier.visual);
      } else if (unit.motion == QStringLiteral("idle") && visual_moving) {
        rpg_locomotion_mismatch[group] =
            QStringLiteral("simulation reported idle at %1 s while the renderer showed "
                           "%2")
                .arg(frame.time_seconds, 0, 'f', 2)
                .arg(soldier.visual);
      }
    }
  }
}

void ArenaScenarioRunner::Impl::observe_rpg_swing_cadence(const TraceFrame& frame) {
  if (scenario.rpg_commander_group.isEmpty()) {
    return;
  }
  const QString& group = scenario.rpg_commander_group;
  for (auto const& unit : frame.units) {
    if (unit.group != group) {
      continue;
    }
    int const striking =
        static_cast<int>(Engine::Core::RpgCommanderActionPhase::Strike);
    int const previous_phase = rpg_action_phase_previous.value(group, 0);
    float const previous_time = rpg_action_time_previous.value(group, 1.0F);

    bool const swing_started =
        unit.rpg_action_phase == striking &&
        (previous_phase != striking || unit.rpg_action_normalized_time < previous_time);

    auto const bank_carry = [&]() {
      if (rpg_swing_carry_open.value(group, false)) {
        rpg_swing_carry[group].push_back(rpg_swing_carry_pending.value(group, 0.0F));
        rpg_swing_carry_open[group] = false;
      }
    };
    if (swing_started) {
      bank_carry();
      rpg_swing_starts[group].push_back(frame.time_seconds);
      rpg_swing_carry_pending[group] = 0.0F;
      rpg_swing_carry_origin[group] = unit.position;
      rpg_swing_carry_open[group] = true;
    }
    if (rpg_swing_carry_open.value(group, false)) {
      if (unit.rpg_action_phase == striking) {
        rpg_swing_carry_pending[group] +=
            horizontal_distance(rpg_swing_carry_origin.value(group), unit.position);
        rpg_swing_carry_origin[group] = unit.position;
      } else {
        bank_carry();
      }
    }
    rpg_action_phase_previous[group] = unit.rpg_action_phase;
    rpg_action_time_previous[group] = unit.rpg_action_normalized_time;
  }
}

auto ArenaScenarioRunner::Impl::travel_key(const ArenaExpectation& expectation)
    -> QString {
  return QStringLiteral("%1|%2|%3")
      .arg(expectation.group)
      .arg(expectation.start_seconds)
      .arg(expectation.end_seconds);
}

void ArenaScenarioRunner::Impl::observe_rpg_travel(const TraceFrame& frame) {
  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind != ArenaExpectationKind::RpgTravelObserved) {
      continue;
    }
    for (auto const& unit : frame.units) {
      if (unit.group != expectation.group) {
        continue;
      }
      auto& observation = rpg_travel_observations[travel_key(expectation)];
      if (!observation.has_start && frame.time_seconds >= expectation.start_seconds) {
        observation.has_start = true;
        observation.start = unit.position;
      }
      if (observation.has_start && frame.time_seconds <= expectation.end_seconds) {
        observation.has_end = true;
        observation.end = unit.position;
      }
    }
  }
}

void ArenaScenarioRunner::Impl::observe_group_pair_proximity(const TraceFrame& frame) {
  for (auto const& expectation : scenario.expectations) {
    bool const keeps_apart =
        expectation.kind == ArenaExpectationKind::GroupPairKeepsApart;
    if (expectation.kind != ArenaExpectationKind::RpgApproachWithin && !keeps_apart) {
      continue;
    }
    if (keeps_apart && (frame.time_seconds < expectation.start_seconds ||
                        (expectation.end_seconds > 0.0F &&
                         frame.time_seconds > expectation.end_seconds))) {
      continue;
    }
    float closest = std::numeric_limits<float>::infinity();
    for (auto const& unit : frame.units) {
      if (unit.group != expectation.group) {
        continue;
      }
      for (auto const& other : frame.units) {
        if (other.group != expectation.target_group) {
          continue;
        }
        closest = std::min(closest, horizontal_distance(unit.position, other.position));
      }
    }
    if (!std::isfinite(closest)) {
      continue;
    }
    const QString key =
        projectile_pair_key(expectation.group, expectation.target_group);
    auto& minima =
        keeps_apart ? minimum_group_pair_standoff : minimum_group_pair_distance;
    auto const existing = minima.constFind(key);
    if (existing == minima.cend() || closest < existing.value()) {
      minima[key] = closest;
      if (keeps_apart) {
        minimum_group_pair_standoff_at[key] = frame.time_seconds;
      }
    }
  }
}

void ArenaScenarioRunner::Impl::observe_soldiers(Engine::Core::EntityID entity_id,
                                                 const QString& group,
                                                 TraceFrame& frame) {
  auto const* presentation =
      world.try_get<Engine::Core::FormationPresentationComponent>(entity_id);
  auto const* debug =
      Render::Profiling::CombatAnimationDiagnostics::instance().find_unit(entity_id);
  const bool verify_render_continuity = std::any_of(
      scenario.expectations.begin(),
      scenario.expectations.end(),
      [&](const ArenaExpectation& expectation) {
        return expectation.kind == ArenaExpectationKind::NoRenderVisibilityChurn &&
               expectation_active(expectation) && applies_to(expectation, group);
      });

  const bool verify_unit_submission =
      verify_render_continuity ||
      std::any_of(scenario.expectations.begin(),
                  scenario.expectations.end(),
                  [&](const ArenaExpectation& expectation) {
                    return expectation.kind ==
                               ArenaExpectationKind::RpgFormationSurvivesLensGap &&
                           expectation_active(expectation) &&
                           applies_to(expectation, group);
                  });
  if (debug == nullptr) {
    const auto previous_samples = sampled_soldiers_by_entity.value(entity_id);
    const bool had_living_render_sample = std::any_of(
        previous_samples.begin(), previous_samples.end(), [&](int soldier_index) {
          const auto state =
              soldier_states.constFind(soldier_key(entity_id, soldier_index));
          return state != soldier_states.cend() && state->alive;
        });
    auto const unit_cull_reason =
        Render::Profiling::CombatAnimationDiagnostics::instance().unit_cull_reason(
            entity_id);

    const bool explained_by_visibility =
        unit_cull_reason == Render::Profiling::SoldierCullReason::Frustum ||
        unit_cull_reason == Render::Profiling::SoldierCullReason::Fog;
    const bool unit_continuity_required =
        verify_render_continuity ||
        (verify_unit_submission && !explained_by_visibility);
    if (unit_continuity_required && entity_alive(entity_id) &&
        entities_with_render_samples.contains(entity_id) && had_living_render_sample) {
      add_issue(QStringLiteral("unit_submission_disappeared"),
                QStringLiteral("%1 entity %2 disappeared before per-soldier "
                               "submission diagnostics (%3)")
                    .arg(group)
                    .arg(entity_id)
                    .arg(QString::fromLatin1(
                        Render::Profiling::soldier_cull_reason_name(unit_cull_reason))),
                entity_id);
    }
    return;
  }
  if (world.has<Engine::Core::ElephantComponent>(entity_id) &&
      debug->unit.is_attacking) {
    visible_attacks[group] = true;
    useful_bot_action[group] = true;
    auto const* contact =
        world.try_get<Engine::Core::FormationContactComponent>(entity_id);
    if (contact != nullptr &&
        std::any_of(contact->fronts.begin(),
                    contact->fronts.end(),
                    [](auto const& front) { return front.in_contact; })) {
      paired_visible_attacks[group] = true;
    }
  }
  entities_with_render_samples.insert(entity_id);
  QSet<int> sampled_this_frame;
  bool const formation_fight_active =
      presentation != nullptr && presentation->melee_ordered &&
      presentation->target_alive &&
      std::any_of(
          presentation->soldiers.begin(),
          presentation->soldiers.end(),
          [](auto const& soldier) {
            return soldier.alive &&
                   (soldier.action ==
                        Engine::Core::FormationSoldierAction::MeleeEngaged ||
                    soldier.action ==
                        Engine::Core::FormationSoldierAction::MeleeFollowThrough);
          });
  int living_soldier_samples = 0;
  int lens_gap_culled_samples = 0;
  float minimum_attack_phase = std::numeric_limits<float>::max();
  float maximum_attack_phase = std::numeric_limits<float>::lowest();
  QSet<int> attack_phase_bins;
  int visible_attack_count = 0;
  for (auto const& soldier : debug->soldiers) {
    sampled_this_frame.insert(soldier.soldier_index);
    Engine::Core::FormationSoldierPresentation const* directive = nullptr;
    if (presentation != nullptr && soldier.soldier_index >= 0) {
      auto const slot = static_cast<std::size_t>(soldier.soldier_index);
      if (slot < presentation->soldiers.size() &&
          presentation->soldiers[slot].slot_index == slot) {
        directive = &presentation->soldiers[slot];
      }
    }
    QString declared_action = QStringLiteral("single_body");
    if (directive != nullptr) {
      switch (directive->action) {
      case Engine::Core::FormationSoldierAction::FollowUnit:
        declared_action = QStringLiteral("follow_unit");
        break;
      case Engine::Core::FormationSoldierAction::MeleeReady:
        declared_action = QStringLiteral("melee_ready");
        break;
      case Engine::Core::FormationSoldierAction::MeleeEngaged:
        declared_action = QStringLiteral("melee_engaged");
        break;
      case Engine::Core::FormationSoldierAction::MeleeFollowThrough:
        declared_action = QStringLiteral("melee_follow_through");
        break;
      case Engine::Core::FormationSoldierAction::MeleeGuard:
        declared_action = QStringLiteral("melee_guard");
        break;
      case Engine::Core::FormationSoldierAction::MeleeReposition:
        declared_action = QStringLiteral("melee_reposition");
        break;
      }
    }
    ++report.rendered_soldier_samples;
    ++rendered_by_group[group];
    bool const culled =
        soldier.cull_reason != Render::Profiling::SoldierCullReason::None;
    bool const observed_attack =
        soldier.visual_state == Render::Profiling::SoldierVisualState::Attack &&
        !culled;
    bool const observed_movement =
        (soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
         soldier.visual_state == Render::Profiling::SoldierVisualState::Run) &&
        !culled;
    if (observed_movement) {
      visible_movement[group] = true;
    }
    if (!culled &&
        (soldier.visual_state == Render::Profiling::SoldierVisualState::HitReaction ||
         (soldier.is_swing_recoiling &&
          std::abs(soldier.hit_reaction_tilt_degrees) > 0.05F))) {

      visible_hit_reactions[group] = true;
    }
    if ((soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
         soldier.visual_state == Render::Profiling::SoldierVisualState::Dead) &&
        !culled) {
      visible_deaths[group] = true;
    }
    frame.soldiers.push_back(
        {entity_id,
         soldier.soldier_index,
         soldier.root_position,
         soldier.root_yaw_degrees,
         soldier.root_up_y,
         soldier.submitted_body_up_y,
         soldier.submitted_max_arm_reach,
         soldier.submitted_body_pose_valid,
         soldier.foot_l_world,
         soldier.foot_r_world,
         soldier.hand_l_world,
         soldier.hand_r_world,
         soldier.locomotion_blend,
         soldier.locomotion_presence,
         soldier.cycle_phase,
         soldier.travel_alignment,
         soldier.travel_lateral_share,
         soldier.action_link_weight,
         soldier.persistent_valid,
         soldier.sample_time,
         soldier.persistent_last_sample_time,
         declared_action,
         directive != nullptr &&
                 (directive->action ==
                      Engine::Core::FormationSoldierAction::MeleeEngaged ||
                  directive->action ==
                      Engine::Core::FormationSoldierAction::MeleeFollowThrough)
             ? static_cast<int>(directive->target_slot)
             : -1,
         directive != nullptr ? directive->engagement_surface_gap : 0.0F,
         QString::fromLatin1(
             Render::Profiling::animation_state_name(soldier.animation_state)),
         QString::fromLatin1(
             Render::Profiling::soldier_visual_state_name(soldier.visual_state)),
         soldier.is_swing_recoiling,
         soldier.hit_reaction_tilt_degrees,
         soldier.attack_phase,
         soldier.transitions_last_second,
         culled,
         QString::fromLatin1(
             Render::Profiling::soldier_cull_reason_name(soldier.cull_reason)),
         static_cast<int>(soldier.lod),
         soldier.pelvis_yaw_degrees,
         soldier.torso_yaw_degrees,
         soldier.attack_is_melee});

    std::uint64_t const key = soldier_key(entity_id, soldier.soldier_index);
    const bool continuity_alive =
        soldier.visual_state != Render::Profiling::SoldierVisualState::Dying &&
        soldier.visual_state != Render::Profiling::SoldierVisualState::Dead &&
        (directive == nullptr || directive->alive);
    if (directive != nullptr && directive->alive) {
      living_soldiers_by_group[group].insert(key);
      if (directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged) {
        engaged_soldiers_by_group[group].insert(key);
      }
    }
    if (continuity_alive) {
      ++living_soldier_samples;
      if (soldier.cull_reason == Render::Profiling::SoldierCullReason::LensGap) {
        ++lens_gap_culled_samples;
      }
    }
    auto& previous = soldier_states[key];
    if (verify_render_continuity && continuity_alive && culled &&
        previous.ever_visible && (!previous.initialized || !previous.culled)) {
      add_issue(
          QStringLiteral("soldier_submission_disappeared"),
          QStringLiteral("%1 entity %2 soldier %3 changed from submitted to "
                         "culled (%4)")
              .arg(group)
              .arg(entity_id)
              .arg(soldier.soldier_index)
              .arg(QString::fromLatin1(
                  Render::Profiling::soldier_cull_reason_name(soldier.cull_reason))),
          entity_id,
          soldier.soldier_index);
    }
    if (!culled) {
      previous.ever_visible = true;
    }
    if ((observed_attack && (!previous.initialized || !previous.attacking)) ||
        soldier.attack_phase_reset) {
      ++attack_entries_by_soldier[key];
    }
    bool const recovered_to_controlled_pose =
        soldier.visual_state == Render::Profiling::SoldierVisualState::Idle ||
        soldier.visual_state == Render::Profiling::SoldierVisualState::Hold ||
        soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
        soldier.visual_state == Render::Profiling::SoldierVisualState::Run;
    if (!culled && previous.completed_attack_phase && recovered_to_controlled_pose) {
      visible_attack_recoveries[group] = true;
    }
    if (observed_attack && soldier.attack_phase >= 0.85F) {
      previous.completed_attack_phase = true;
    }
    bool const living_formation_fighter =
        formation_fight_active && directive != nullptr && directive->alive && !culled;
    bool const observed_guard =
        living_formation_fighter &&
        soldier.visual_state == Render::Profiling::SoldierVisualState::Hold;
    if (observed_guard) {
      guarding_soldiers_by_group[group].insert(key);
    }
    if (!living_formation_fighter) {
      previous.fight_idle_since = -1.0F;
      previous.terminal_pose_since = -1.0F;
    }
    float const step =
        previous.initialized
            ? horizontal_distance(soldier.root_position, previous.root_position)
            : 0.0F;
    for (auto const& expectation : scenario.expectations) {
      if (!expectation_active(expectation) || !applies_to(expectation, group)) {
        continue;
      }
      if (expectation.kind == ArenaExpectationKind::NoPoseOscillation &&
          soldier.churn_flagged) {
        add_issue(QStringLiteral("pose_oscillation"),
                  QStringLiteral("%1 entity %2 soldier %3 changed visual state %4 "
                                 "times in one second")
                      .arg(group)
                      .arg(entity_id)
                      .arg(soldier.soldier_index)
                      .arg(soldier.transitions_last_second),
                  entity_id,
                  soldier.soldier_index);
      }
      if (expectation.kind == ArenaExpectationKind::FullCreatureDetailOnly &&
          continuity_alive) {
        const bool lod_shed =
            soldier.cull_reason == Render::Profiling::SoldierCullReason::Distance;
        const bool reduced_visible_mesh =
            !culled && soldier.lod != static_cast<std::uint8_t>(
                                          Render::Creature::CreatureLOD::Full);
        if (lod_shed || reduced_visible_mesh) {
          add_issue(
              QStringLiteral("ultra_creature_lod_used"),
              QStringLiteral("%1 entity %2 soldier %3 used LOD %4 (%5) while "
                             "full creature detail was required")
                  .arg(group)
                  .arg(entity_id)
                  .arg(soldier.soldier_index)
                  .arg(static_cast<int>(soldier.lod))
                  .arg(QString::fromLatin1(Render::Profiling::soldier_cull_reason_name(
                      soldier.cull_reason))),
              entity_id,
              soldier.soldier_index);
        }
      }

      float const frame_budget_scale =
          std::max(1.0F, (elapsed - previous.observed_at) * 60.0F);

      if (expectation.kind == ArenaExpectationKind::NoRootTeleport &&
          previous.initialized && !previous.culled && !culled &&
          elapsed >= k_spawn_settle_seconds &&
          elapsed - previous.observed_at <= 0.05F) {
        float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                            : k_default_root_step) *
                              frame_budget_scale;
        if (step > allowed) {
          add_issue(QStringLiteral("render_root_teleport"),
                    QStringLiteral("%1 entity %2 soldier %3 render root jumped %4 m "
                                   "between frames")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(step, 0, 'f', 2),
                    entity_id,
                    soldier.soldier_index);
        }
      }
      if (expectation.kind == ArenaExpectationKind::NoUnexpectedFallPose && !culled) {
        float const minimum_up =
            expectation.threshold > 0.0F ? expectation.threshold : k_default_fall_up_y;
        bool const legitimate_fall =
            soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
            soldier.visual_state == Render::Profiling::SoldierVisualState::Dead ||
            (soldier.visual_state ==
                 Render::Profiling::SoldierVisualState::HitReaction &&
             soldier.hit_reaction_kind == Engine::Core::HitReactionKind::Evade);
        float const observed_up = soldier.submitted_body_pose_valid
                                      ? soldier.submitted_body_up_y
                                      : soldier.root_up_y;
        if (observed_up < minimum_up && !legitimate_fall) {
          add_issue(
              QStringLiteral("unexpected_fall_pose"),
              QStringLiteral("%1 entity %2 soldier %3 submitted body up-vector was "
                             "%4 while alive")
                  .arg(group)
                  .arg(entity_id)
                  .arg(soldier.soldier_index)
                  .arg(observed_up, 0, 'f', 2),
              entity_id,
              soldier.soldier_index);
        }
      }
      if (expectation.kind == ArenaExpectationKind::NoLimbOverextension && !culled &&
          soldier.submitted_body_pose_valid) {
        float const maximum_reach =
            expectation.threshold > 0.0F ? expectation.threshold : 0.60F;
        if (soldier.submitted_max_arm_reach > maximum_reach) {
          add_issue(QStringLiteral("limb_overextension"),
                    QStringLiteral("%1 entity %2 soldier %3 submitted arm reach was "
                                   "%4 m")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(soldier.submitted_max_arm_reach, 0, 'f', 2),
                    entity_id,
                    soldier.soldier_index);
        }
      }
      bool const dying_now =
          soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Dead;
      bool const joints_comparable = previous.initialized && previous.joints_valid &&
                                     soldier.joint_sample_valid && !previous.culled &&
                                     !culled && elapsed - previous.observed_at <= 0.05F;

      if (expectation.kind == ArenaExpectationKind::NoPlantedFootSliding &&
          joints_comparable) {
        bool const locomoting =
            soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
            soldier.visual_state == Render::Profiling::SoldierVisualState::Run ||
            soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
            soldier.visual_state == Render::Profiling::SoldierVisualState::Dead ||
            std::max(soldier.locomotion_presence, previous.locomotion_presence) >
                k_stride_fade_presence;
        bool const root_planted = step <= k_planted_root_step;
        if (!locomoting && root_planted) {
          float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                              : k_default_foot_slide) *
                                frame_budget_scale;
          float const ground_y = soldier.root_position.y();
          float planted_slide = 0.0F;
          auto const consider_foot = [&](const QVector3D& now,
                                         const QVector3D& before) {
            bool const planted_now = now.y() - ground_y <= k_planted_foot_height;
            bool const planted_before = before.y() - ground_y <= k_planted_foot_height;
            if (!planted_now || !planted_before) {
              return;
            }
            planted_slide = std::max(planted_slide, horizontal_distance(now, before));
          };
          consider_foot(soldier.foot_l_world, previous.foot_l_world);
          consider_foot(soldier.foot_r_world, previous.foot_r_world);

          if (planted_slide > allowed) {
            add_issue(QStringLiteral("planted_foot_slide"),
                      QStringLiteral("%1 entity %2 soldier %3 planted foot slid %4 m "
                                     "between frames while not walking")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(planted_slide, 0, 'f', 3),
                      entity_id,
                      soldier.soldier_index);
          }
        }
      }

      if (expectation.kind == ArenaExpectationKind::NoWeaponTeleport &&
          joints_comparable) {
        float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                            : k_default_hand_step) *
                              frame_budget_scale;
        float const step_l = (soldier.hand_l_world - previous.hand_l_world).length();
        float const step_r = (soldier.hand_r_world - previous.hand_r_world).length();
        float const worst = std::max(step_l, step_r);
        if (worst > allowed) {
          add_issue(QStringLiteral("weapon_hand_teleport"),
                    QStringLiteral("%1 entity %2 soldier %3 weapon hand jumped %4 m "
                                   "between frames")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(worst, 0, 'f', 3),
                    entity_id,
                    soldier.soldier_index);
        }
      }

      if (expectation.kind == ArenaExpectationKind::NoPelvisSnap && joints_comparable &&
          !dying_now && !previous.dying) {
        float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                            : k_default_pelvis_step) *
                              frame_budget_scale;
        float const turn = std::abs(
            shortest_degrees(soldier.pelvis_yaw_degrees, previous.pelvis_yaw_degrees));
        if (turn > allowed) {
          add_issue(QStringLiteral("pelvis_snap"),
                    QStringLiteral("%1 entity %2 soldier %3 pelvis rotated %4 degrees "
                                   "between frames")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(turn, 0, 'f', 1),
                    entity_id,
                    soldier.soldier_index);
        }
      }

      if (expectation.kind == ArenaExpectationKind::NoLocomotionRestart &&
          joints_comparable && !dying_now && !previous.dying &&
          soldier.locomotion_presence >= k_locomotion_restart_presence &&
          previous.locomotion_presence >= k_locomotion_restart_presence) {
        float const dt = std::max(elapsed - previous.observed_at, 1.0F / 60.0F);
        float const allowed =
            (expectation.threshold > 0.0F ? expectation.threshold
                                          : dt * k_fastest_cycle_phase_rate) +
            k_locomotion_phase_slack;
        float const jump =
            std::abs(wrapped_phase_delta(soldier.cycle_phase, previous.cycle_phase));
        if (jump > allowed) {
          add_issue(QStringLiteral("locomotion_phase_restart"),
                    QStringLiteral("%1 entity %2 soldier %3 stride phase jumped from "
                                   "%4 to %5 while still walking")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(previous.cycle_phase, 0, 'f', 3)
                        .arg(soldier.cycle_phase, 0, 'f', 3),
                    entity_id,
                    soldier.soldier_index);
        }
      }

      if (expectation.kind == ArenaExpectationKind::NoAttackRestart && !culled &&
          observed_attack && !soldier.is_hit_reacting && !soldier.is_swing_recoiling) {
        float const restart_phase = expectation.threshold > 0.0F
                                        ? expectation.threshold
                                        : k_attack_restart_phase;
        bool const reset_in_place = soldier.attack_phase_reset && previous.attacking &&
                                    previous.attack_phase < restart_phase;
        bool const re_entered =
            !previous.attacking && !previous.hit_since_attack_exit &&
            !previous.walked_since_attack_exit && previous.attack_exit_at >= 0.0F &&
            elapsed - previous.attack_exit_at <= k_attack_restart_window_seconds &&
            previous.attack_exit_phase < restart_phase &&
            previous.attack_exit_is_melee == soldier.attack_is_melee &&
            soldier.attack_phase + k_attack_resume_tolerance <
                previous.attack_exit_phase;
        if (reset_in_place || re_entered) {
          float const cut_at =
              reset_in_place ? previous.attack_phase : previous.attack_exit_phase;
          add_issue(QStringLiteral("attack_presentation_restart"),
                    QStringLiteral("%1 entity %2 soldier %3 restarted its swing from "
                                   "phase %4 before the strike landed")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(cut_at, 0, 'f', 2),
                    entity_id,
                    soldier.soldier_index);
        }
      }

      if (expectation.kind == ArenaExpectationKind::NoBodyPoseSnap &&
          joints_comparable && !dying_now && !previous.dying) {
        float const swing_step = observed_attack && previous.attacking
                                     ? k_swing_body_pose_step
                                     : k_default_body_pose_step;
        float const allowed =
            (expectation.threshold > 0.0F ? expectation.threshold : swing_step) *
            frame_budget_scale;
        auto const local = body_local_joints(soldier);
        float worst = 0.0F;
        std::size_t worst_joint = 0;
        for (std::size_t joint = 0; joint < local.size(); ++joint) {
          float const moved = (local[joint] - previous.local_joints[joint]).length();
          float const continuing = k_pose_snap_acceleration *
                                       previous.local_joint_steps[joint] *
                                       frame_budget_scale +
                                   k_pose_snap_margin;
          if (moved > continuing && moved > worst) {
            worst = moved;
            worst_joint = joint;
          }
        }
        static constexpr std::array<const char*, 4> k_joint_names{
            "left hand", "right hand", "left foot", "right foot"};
        if (worst > allowed) {
          add_issue(
              QStringLiteral("body_pose_snap"),
              QStringLiteral("%1 entity %2 soldier %3 %4 jumped %5 m in "
                             "the body frame between frames (%6)")
                  .arg(group)
                  .arg(entity_id)
                  .arg(soldier.soldier_index)
                  .arg(QString::fromLatin1(k_joint_names[worst_joint]))
                  .arg(worst, 0, 'f', 3)
                  .arg(QString::fromLatin1(Render::Profiling::animation_state_name(
                      soldier.animation_state))),
              entity_id,
              soldier.soldier_index);
        }
      }

      if (expectation.kind == ArenaExpectationKind::AttackHasTorsoRotation &&
          soldier.joint_sample_valid && !culled) {
        if (observed_attack) {
          if (!previous.attack_yaw_tracked) {
            previous.attack_pelvis_yaw_min = soldier.torso_yaw_degrees;
            previous.attack_pelvis_yaw_max = soldier.torso_yaw_degrees;
            previous.attack_yaw_tracked = true;
          } else {
            float const relative = shortest_degrees(soldier.torso_yaw_degrees,
                                                    previous.attack_pelvis_yaw_min);
            previous.attack_pelvis_yaw_max =
                std::max(previous.attack_pelvis_yaw_max,
                         previous.attack_pelvis_yaw_min + relative);
            previous.attack_pelvis_yaw_min =
                std::min(previous.attack_pelvis_yaw_min,
                         previous.attack_pelvis_yaw_min + relative);
          }
        } else if (previous.attack_yaw_tracked) {
          float const swept =
              previous.attack_pelvis_yaw_max - previous.attack_pelvis_yaw_min;
          float const required = expectation.threshold > 0.0F
                                     ? expectation.threshold
                                     : k_default_attack_torso_sweep;
          if (swept < required) {
            add_issue(QStringLiteral("attack_without_torso_rotation"),
                      QStringLiteral("%1 entity %2 soldier %3 swung with only %4 "
                                     "degrees of torso rotation")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(swept, 0, 'f', 1),
                      entity_id,
                      soldier.soldier_index);
          }
          previous.attack_yaw_tracked = false;
        }
      }

      if (expectation.kind == ArenaExpectationKind::AllLivingSoldiersFight &&
          living_formation_fighter) {
        if (observed_attack || observed_guard ||
            soldier.visual_state ==
                Render::Profiling::SoldierVisualState::HitReaction) {
          previous.fight_idle_since = -1.0F;
        } else if (previous.fight_idle_since < 0.0F) {
          previous.fight_idle_since = elapsed;
        } else {
          float const allowed_idle =
              expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
          if (elapsed - previous.fight_idle_since > allowed_idle) {
            add_issue(QStringLiteral("living_soldier_idle_in_fight"),
                      QStringLiteral("%1 entity %2 soldier %3 stayed out of its "
                                     "unit fight animation for %4 s")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(elapsed - previous.fight_idle_since, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        bool const terminal_attack_pose =
            observed_attack && soldier.attack_phase >= 0.99F;
        if (!terminal_attack_pose) {
          previous.terminal_pose_since = -1.0F;
        } else if (previous.terminal_pose_since < 0.0F) {
          previous.terminal_pose_since = elapsed;
        } else if (elapsed - previous.terminal_pose_since > 0.25F) {
          add_issue(QStringLiteral("fight_animation_terminal_stall"),
                    QStringLiteral("%1 entity %2 soldier %3 held its terminal "
                                   "fight pose for %4 s")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(elapsed - previous.terminal_pose_since, 0, 'f', 2),
                    entity_id,
                    soldier.soldier_index);
        }
      }
      if (expectation.kind == ArenaExpectationKind::HoldPoseMaintained && !culled) {
        auto const* hold = world.try_get<Engine::Core::HoldModeComponent>(entity_id);
        if (hold != nullptr && hold->active && hold->kneel_entry_progress >= 0.999F &&
            soldier.animation_state != Render::Creature::AnimationStateId::Hold) {
          add_issue(
              QStringLiteral("hold_pose_replaced"),
              QStringLiteral("%1 entity %2 soldier %3 rendered %4 while Hold was "
                             "active")
                  .arg(group)
                  .arg(entity_id)
                  .arg(soldier.soldier_index)
                  .arg(QString::fromLatin1(Render::Profiling::animation_state_name(
                      soldier.animation_state))),
              entity_id,
              soldier.soldier_index);
        }
      }
    }
    if (observed_attack) {
      attacking_soldiers_by_group[group].insert(key);
      minimum_attack_phase = std::min(minimum_attack_phase, soldier.attack_phase);
      maximum_attack_phase = std::max(maximum_attack_phase, soldier.attack_phase);
      attack_phase_bins.insert(
          std::clamp(static_cast<int>(soldier.attack_phase * 12.0F), 0, 11));
      ++visible_attack_count;
      visible_attacks[group] = true;
      useful_bot_action[group] = true;
      if (presentation != nullptr && presentation->melee_ordered) {
        bool const physical_strike =
            directive != nullptr &&
            directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged;
        bool const declared_follow_through =
            directive != nullptr &&
            directive->action ==
                Engine::Core::FormationSoldierAction::MeleeFollowThrough;
        bool const continuing_visible_attack =
            declared_follow_through && previous.initialized && previous.attacking;

        if (physical_strike || continuing_visible_attack) {
          paired_visible_attacks[group] = true;
        }
      } else {

        paired_visible_attacks[group] = true;
      }
      if (auto response = responses.find(entity_id); response != responses.end()) {
        response->observed = true;
      }
    }
    previous.root_position = soldier.root_position;
    previous.hand_l_world = soldier.hand_l_world;
    previous.hand_r_world = soldier.hand_r_world;
    previous.foot_l_world = soldier.foot_l_world;
    previous.foot_r_world = soldier.foot_r_world;
    previous.pelvis_yaw_degrees = soldier.pelvis_yaw_degrees;
    previous.locomotion_presence = soldier.locomotion_presence;
    previous.cycle_phase = soldier.cycle_phase;
    if (previous.attacking && !observed_attack && !culled) {
      previous.attack_exit_phase = previous.attack_phase;
      previous.attack_exit_at = elapsed;
      previous.attack_exit_is_melee = previous.attack_is_melee;
      previous.hit_since_attack_exit = false;
      previous.walked_since_attack_exit = false;
    }
    if (soldier.locomotion_presence >= k_locomotion_restart_presence) {
      previous.walked_since_attack_exit = true;
    }
    if (soldier.is_hit_reacting || soldier.is_swing_recoiling) {
      previous.hit_since_attack_exit = true;
    }
    previous.attack_phase = soldier.attack_phase;
    if (soldier.joint_sample_valid) {
      auto const local = body_local_joints(soldier);
      bool const steps_comparable = previous.initialized && previous.joints_valid &&
                                    !previous.culled && !culled &&
                                    elapsed - previous.observed_at <= 0.05F;
      for (std::size_t joint = 0; joint < local.size(); ++joint) {
        previous.local_joint_steps[joint] =
            steps_comparable
                ? (local[joint] - previous.local_joints[joint]).length() /
                      std::max(1.0F, (elapsed - previous.observed_at) * 60.0F)
                : 0.0F;
      }
      previous.local_joints = local;
    }
    previous.dying =
        soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
        soldier.visual_state == Render::Profiling::SoldierVisualState::Dead;
    previous.joints_valid = soldier.joint_sample_valid;
    previous.observed_at = elapsed;
    previous.initialized = true;
    previous.culled = culled;
    previous.attacking = observed_attack;
    previous.attack_is_melee = soldier.attack_is_melee;
    previous.alive = continuity_alive;
  }

  if (verify_render_continuity) {
    const auto previous_samples = sampled_soldiers_by_entity.value(entity_id);
    for (int const soldier_index : previous_samples) {
      if (sampled_this_frame.contains(soldier_index)) {
        continue;
      }
      const auto previous =
          soldier_states.constFind(soldier_key(entity_id, soldier_index));
      bool still_alive = previous != soldier_states.cend() && previous->alive;
      if (presentation != nullptr && soldier_index >= 0) {
        const auto slot = static_cast<std::size_t>(soldier_index);
        still_alive = still_alive && slot < presentation->soldiers.size() &&
                      presentation->soldiers[slot].alive;
      }
      if (still_alive) {
        add_issue(QStringLiteral("soldier_submission_missing"),
                  QStringLiteral("%1 entity %2 soldier %3 vanished from the "
                                 "render sample set")
                      .arg(group)
                      .arg(entity_id)
                      .arg(soldier_index),
                  entity_id,
                  soldier_index);
      }
    }
  }
  check_lens_gap_readability(
      entity_id, group, living_soldier_samples, lens_gap_culled_samples);
  sampled_soldiers_by_entity[entity_id] = std::move(sampled_this_frame);
  bool const small_group_staggered =
      visible_attack_count >= 2 && visible_attack_count < 8 &&
      maximum_attack_phase - minimum_attack_phase >= 0.02F;
  bool const formation_staggered = visible_attack_count >= 8 &&
                                   attack_phase_bins.size() >= 4 &&
                                   maximum_attack_phase - minimum_attack_phase >= 0.20F;
  if (small_group_staggered || formation_staggered) {
    staggered_attack_phases[group] = true;
  }
}

void ArenaScenarioRunner::Impl::check_lens_gap_readability(
    Engine::Core::EntityID entity_id,
    const QString& group,
    int living_samples,
    int lens_gap_culled) {
  if (living_samples <= 0 || lens_gap_culled <= 0) {
    return;
  }
  for (auto const& expectation : scenario.expectations) {
    if (expectation.kind != ArenaExpectationKind::RpgFormationSurvivesLensGap ||
        !expectation_active(expectation) || !applies_to(expectation, group)) {
      continue;
    }
    float const allowed_fraction =
        expectation.threshold > 0.0F ? expectation.threshold : 0.5F;
    float const culled_fraction =
        static_cast<float>(lens_gap_culled) / static_cast<float>(living_samples);
    if (culled_fraction > allowed_fraction) {
      add_issue(QStringLiteral("formation_erased_by_lens_gap"),
                QStringLiteral("%1 entity %2 dropped %3 of %4 living soldiers to "
                               "the chase-lens gap in one frame")
                    .arg(group)
                    .arg(entity_id)
                    .arg(lens_gap_culled)
                    .arg(living_samples),
                entity_id);
    }
  }
}

} // namespace Arena
