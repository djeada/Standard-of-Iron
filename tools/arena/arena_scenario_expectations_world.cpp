#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

constexpr float k_default_frame_budget_ms = 33.34F;

} // namespace

void ArenaScenarioRunner::Impl::check_movement_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::GroupIsRendered:
    if (rendered_by_group.value(expectation.group, 0U) == 0U) {
      add_issue(QStringLiteral("group_not_rendered"),
                QStringLiteral("%1 produced no rendered soldier observations")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::GroupExists:
    if (ids(expectation.group).empty() || group_destroyed(expectation.group)) {
      add_issue(QStringLiteral("group_missing"),
                QStringLiteral("%1 did not retain a live scenario entity")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::AutoEngagementObserved: {
    QStringList passive;
    for (auto entity_id : ids(expectation.group)) {
      if (!auto_engaged_entities.contains(entity_id)) {
        passive.append(QString::number(entity_id));
      }
    }
    if (passive.isEmpty() && ids(expectation.group).empty()) {
      passive.append(QStringLiteral("none spawned"));
    }
    if (!passive.isEmpty()) {
      add_issue(QStringLiteral("no_auto_engagement"),
                QStringLiteral("%1 never engaged a hostile on its own initiative "
                               "(passive: %2)")
                    .arg(expectation.group, passive.join(QStringLiteral(", "))));
    }
    break;
  }
  case ArenaExpectationKind::NoAutoEngagementObserved:
  case ArenaExpectationKind::EngagementReleasedByOrder:

    break;

  case ArenaExpectationKind::GroupDestroyed:
    if (!group_destroyed(expectation.group)) {
      add_issue(QStringLiteral("group_not_destroyed"),
                QStringLiteral("%1 retained a living scenario entity")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::GroupReachedDestination: {
    QVector3D centroid;
    int living = 0;
    for (auto entity_id : ids(expectation.group)) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform != nullptr && entity_alive(entity_id)) {
        centroid += vector_from_transform(*transform);
        ++living;
      }
    }
    float const tolerance = expectation.distance > 0.0F ? expectation.distance : 2.5F;
    QVector3D const destination =
        world_origin + (expectation.position.isNull()
                            ? ordered_destination(expectation.group)
                            : expectation.position);
    if (living == 0 ||
        horizontal_distance(centroid / static_cast<float>(std::max(living, 1)),
                            destination) > tolerance) {
      QVector3D const reached = centroid / static_cast<float>(std::max(living, 1));
      add_issue(QStringLiteral("group_missed_destination"),
                QStringLiteral("%1 did not reach its scenario destination: "
                               "centroid (%2, %3), destination (%4, %5)")
                    .arg(expectation.group)
                    .arg(reached.x(), 0, 'f', 1)
                    .arg(reached.z(), 0, 'f', 1)
                    .arg(destination.x(), 0, 'f', 1)
                    .arg(destination.z(), 0, 'f', 1));
    }
    break;
  }
  case ArenaExpectationKind::NoPermanentStall: {
    float const budget = expectation.threshold > 0.0F ? expectation.threshold : 12.0F;
    QStringList wedged;
    for (auto entity_id : ids(expectation.group)) {
      if (!entity_alive(entity_id)) {
        continue;
      }
      auto const* movement = world.try_get<Engine::Core::MovementComponent>(entity_id);
      auto const* movement_facts =
          world.try_get<Engine::Core::MovementFactsComponent>(entity_id);
      if (movement == nullptr || movement_facts == nullptr) {
        continue;
      }
      auto const& stall = movement_facts->progress.stall;
      float const going_nowhere =
          std::max(stall.stalled_seconds, stall.no_closer_seconds);
      if (movement->get_has_target() && going_nowhere > budget) {
        wedged.push_back(QStringLiteral("%1 (%2 s in %3)")
                             .arg(entity_id)
                             .arg(going_nowhere, 0, 'f', 1)
                             .arg(QString::fromLatin1(Engine::Core::movement_state_name(
                                 movement_facts->progress.state))));
      }
    }
    auto const observation = stall_observations.value(expectation.group);
    if (!wedged.isEmpty()) {
      add_issue(QStringLiteral("permanent_stall"),
                QStringLiteral("%1 ended the run holding an objective it had "
                               "stopped advancing on: %2")
                    .arg(expectation.group, wedged.join(QStringLiteral(", "))));
    } else if (observation.worst_stalled_seconds > budget &&
               observation.recovery_attempts == 0U && observation.abandons == 0U) {

      add_issue(QStringLiteral("unnoticed_stall"),
                QStringLiteral("%1 went nowhere for %2 s at %3 s (%4) without "
                               "the recovery ladder ever engaging")
                    .arg(expectation.group)
                    .arg(observation.worst_stalled_seconds, 0, 'f', 1)
                    .arg(observation.worst_stalled_at, 0, 'f', 1)
                    .arg(observation.worst_state.isEmpty()
                             ? QStringLiteral("unknown state")
                             : observation.worst_state));
    }
    break;
  }
  case ArenaExpectationKind::StallRecoveryObserved: {
    auto const observation = stall_observations.value(expectation.group);
    if (!observation.recovery_seen && observation.abandons == 0U) {
      add_issue(QStringLiteral("stall_recovery_not_exercised"),
                QStringLiteral("%1 never reached the recovery ladder, so this "
                               "scenario proved nothing about it")
                    .arg(expectation.group));
    }
    break;
  }
  case ArenaExpectationKind::GroupHeldOutsideDestination: {
    QVector3D centroid;
    int living = 0;
    for (auto entity_id : ids(expectation.group)) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform != nullptr && entity_alive(entity_id)) {
        centroid += vector_from_transform(*transform);
        ++living;
      }
    }
    float const tolerance = expectation.distance > 0.0F ? expectation.distance : 2.5F;
    QVector3D const destination = world_origin + expectation.position;
    if (living > 0 && horizontal_distance(centroid / static_cast<float>(living),
                                          destination) <= tolerance) {
      add_issue(QStringLiteral("group_passed_barrier"),
                QStringLiteral("%1 reached a destination it should have been "
                               "held out of")
                    .arg(expectation.group));
    }
    break;
  }
  case ArenaExpectationKind::GateOpenedObserved:
    if (!gate_opened_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("gate_never_opened"),
                QStringLiteral("%1 never opened far enough to walk through")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::SiegeTowerDocked:
    if (!tower_docked_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("tower_never_docked"),
                QStringLiteral("%1 never docked against an enemy wall")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::WallWalkerObserved:
    if (!wall_walker_seen) {
      add_issue(QStringLiteral("no_wall_walker"),
                QStringLiteral("no troop was ever seen on a wall-top walkway"));
    }
    break;
  case ArenaExpectationKind::GateRemainedClosed:
    if (!gate_seen.value(expectation.group, false)) {
      add_issue(
          QStringLiteral("gate_not_observed"),
          QStringLiteral("%1 was never sampled as a gate").arg(expectation.group));
    } else if (gate_opened_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("gate_opened_for_enemy"),
                QStringLiteral("%1 opened while only hostile units were near it")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::BridgeTraversalObserved:
    if (!bridge_traversal_seen.value(expectation.group, false)) {
      add_issue(
          QStringLiteral("bridge_not_traversed"),
          QStringLiteral("%1 never occupied the bridge deck").arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::BridgeCenterlineAligned: {
    auto const observation = bridge_alignment.value(expectation.group);
    float const tolerance = expectation.distance > 0.0F ? expectation.distance : 0.50F;
    if (!observation.sampled || observation.lateral_offset > tolerance) {
      add_issue(QStringLiteral("bridge_centerline_missed"),
                observation.sampled
                    ? QStringLiteral("%1 crossed %2 m from the bridge centerline at "
                                     "midspan (allowed %3 m)")
                          .arg(expectation.group)
                          .arg(observation.lateral_offset, 0, 'f', 2)
                          .arg(tolerance, 0, 'f', 2)
                    : QStringLiteral("%1 produced no bridge-midspan alignment sample")
                          .arg(expectation.group));
    }
    break;
  }
  case ArenaExpectationKind::DefensiveUnitLayoutLocked: {
    if (!defensive_layout_locked.value(expectation.group, false)) {
      add_issue(QStringLiteral("defensive_unit_layout_never_locked"),
                QStringLiteral("%1 never locked its internal defensive unit "
                               "layout")
                    .arg(expectation.group));
    }
    break;
  }
  case ArenaExpectationKind::ElevationGainObserved: {
    float const required_gain =
        expectation.threshold > 0.0F ? expectation.threshold : 1.0F;
    float const gain = maximum_elevation.value(expectation.group) -
                       initial_elevation.value(expectation.group);
    if (gain < required_gain) {
      add_issue(QStringLiteral("elevation_gain_not_observed"),
                QStringLiteral("%1 climbed only %2 m (required %3 m)")
                    .arg(expectation.group)
                    .arg(gain, 0, 'f', 2)
                    .arg(required_gain, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::ElevationClimbIsMonotonic:
  case ArenaExpectationKind::ElevationDescentIsMonotonic: {
    bool const climbing =
        expectation.kind == ArenaExpectationKind::ElevationClimbIsMonotonic;
    auto const& legs = climbing ? elevation_climb_legs : elevation_descent_legs;
    auto const leg = legs.value(expectation.group);
    float const tolerance =
        expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
    if (!leg.seeded) {
      add_issue(
          QStringLiteral("elevation_leg_not_sampled"),
          QStringLiteral("%1 produced no elevation sample for its %2 leg")
              .arg(expectation.group)
              .arg(climbing ? QStringLiteral("climb") : QStringLiteral("descent")));
      break;
    }
    if (leg.worst_reversal > tolerance) {
      add_issue(QStringLiteral("elevation_leg_reversed"),
                QStringLiteral("%1 reversed %2 m against its %3 at %4 s (allowed %5 m)")
                    .arg(expectation.group)
                    .arg(leg.worst_reversal, 0, 'f', 2)
                    .arg(climbing ? QStringLiteral("climb") : QStringLiteral("descent"))
                    .arg(leg.worst_at, 0, 'f', 2)
                    .arg(tolerance, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::ElevationHeldAbove: {
    auto const floor = elevation_floors.value(expectation.group);
    if (!floor.seeded) {
      add_issue(QStringLiteral("elevation_floor_not_sampled"),
                QStringLiteral("%1 produced no elevation sample while it was "
                               "meant to be holding the high ground")
                    .arg(expectation.group));
      break;
    }
    if (floor.lowest < expectation.threshold) {
      add_issue(QStringLiteral("elevation_floor_broken"),
                QStringLiteral("%1 dropped to %2 m at %3 s near (%4, %5); it "
                               "was meant to stay above %6 m")
                    .arg(expectation.group)
                    .arg(floor.lowest, 0, 'f', 2)
                    .arg(floor.lowest_at, 0, 'f', 2)
                    .arg(floor.lowest_where.x(), 0, 'f', 2)
                    .arg(floor.lowest_where.z(), 0, 'f', 2)
                    .arg(expectation.threshold, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::NarrowLayoutEngaged: {
    auto const state = narrow_layout.value(expectation.group);
    if (!state.engaged) {
      add_issue(QStringLiteral("narrow_layout_never_engaged"),
                QStringLiteral("%1 crossed with %2 m of corridor against a "
                               "%3 m frontage and never took narrow order")
                    .arg(expectation.group)
                    .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                    .arg(state.formation_half_width * 2.0F, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::NarrowLayoutStaysWide: {
    auto const state = narrow_layout.value(expectation.group);
    if (state.engaged) {
      add_issue(QStringLiteral("narrow_layout_false_positive"),
                QStringLiteral("%1 took narrow order at %2 s with %3 m of "
                               "corridor for a %4 m frontage; it went to %5 "
                               "files and %6 m between files")
                    .arg(expectation.group)
                    .arg(state.engaged_from, 0, 'f', 2)
                    .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                    .arg(state.formation_half_width * 2.0F, 0, 'f', 2)
                    .arg(state.narrowest_files)
                    .arg(state.tightest_file_spacing, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::NarrowLayoutKeepsFiles: {
    auto const state = narrow_layout.value(expectation.group);
    auto const floor_files =
        static_cast<std::uint32_t>(std::max(1.0F, expectation.threshold));
    if (state.engaged && state.narrowest_files < floor_files) {
      add_issue(QStringLiteral("narrow_layout_over_collapsed"),
                QStringLiteral("%1 fell to %2 files (%3) in a %4 m corridor where "
                               "%5 files fit; frontage %6 m, files %7 m apart, "
                               "column %8 m deep")
                    .arg(expectation.group)
                    .arg(state.narrowest_files)
                    .arg(QString::fromLatin1(
                        Engine::Core::traversal_layout_mode_name(state.narrowest_mode)))
                    .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                    .arg(floor_files)
                    .arg(state.narrowest_frontage, 0, 'f', 2)
                    .arg(state.tightest_file_spacing, 0, 'f', 2)
                    .arg(state.deepest_column, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::NarrowLayoutModeSettles: {
    auto const state = narrow_layout.value(expectation.group);
    auto const allowance =
        static_cast<std::uint32_t>(std::max(1.0F, expectation.threshold));
    if (state.mode_changes > allowance) {
      add_issue(QStringLiteral("narrow_layout_oscillated"),
                QStringLiteral("%1 changed layout mode %2 times (allowed %3) "
                               "between %4 m of corridor and a %5 m frontage")
                    .arg(expectation.group)
                    .arg(state.mode_changes)
                    .arg(allowance)
                    .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                    .arg(state.formation_half_width * 2.0F, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::NarrowLayoutRestores: {
    auto const state = narrow_layout.value(expectation.group);
    float const tolerance = expectation.threshold > 0.0F ? expectation.threshold : 0.6F;
    if (state.active_at_end) {
      add_issue(QStringLiteral("narrow_layout_never_released"),
                QStringLiteral("%1 was still in narrow order at the end with "
                               "%2 m of corridor")
                    .arg(expectation.group)
                    .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2));
    } else if (state.files_at_end < state.normal_files) {
      add_issue(QStringLiteral("narrow_layout_kept_files"),
                QStringLiteral("%1 finished on %2 of its %3 files")
                    .arg(expectation.group)
                    .arg(state.files_at_end)
                    .arg(state.normal_files));
    }
    if (state.worst_reform_error > tolerance) {
      add_issue(QStringLiteral("narrow_layout_left_disorder"),
                QStringLiteral("%1 left a soldier %2 m from its slot after "
                               "the passage (allowed %3 m)")
                    .arg(expectation.group)
                    .arg(state.worst_reform_error, 0, 'f', 2)
                    .arg(tolerance, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::SoldiersStayOnWalkableGround: {
    auto const state = off_walkable_soldiers.value(expectation.group);
    int const allowance = static_cast<int>(std::max(0.0F, expectation.threshold));
    if (state.samples > allowance) {
      add_issue(QStringLiteral("soldier_stood_on_blocked_ground"),
                QStringLiteral("%1 drew a soldier on blocked ground %2 times "
                               "(allowed %3), first at %4 s near (%5, %6)")
                    .arg(expectation.group)
                    .arg(state.samples)
                    .arg(allowance)
                    .arg(state.worst_at, 0, 'f', 2)
                    .arg(state.worst.x(), 0, 'f', 2)
                    .arg(state.worst.z(), 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::UnitsStayOnWalkableGround: {
    auto const state = off_walkable_ground.value(expectation.group);
    if (state.samples > 0) {
      add_issue(QStringLiteral("unit_stood_on_blocked_ground"),
                QStringLiteral("%1 stood on blocked ground %2 times, first at "
                               "%3 s near (%4, %5)")
                    .arg(expectation.group)
                    .arg(state.samples)
                    .arg(state.worst_at, 0, 'f', 2)
                    .arg(state.worst.x(), 0, 'f', 2)
                    .arg(state.worst.z(), 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::OwnerCompletesConstruction: {
    int owner_id = 0;
    if (!ids(expectation.group).empty()) {
      auto* anchor = world.get_entity(ids(expectation.group).front());
      auto const* unit = anchor != nullptr
                             ? anchor->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      owner_id = unit != nullptr ? unit->owner_id : 0;
    }
    const int required = std::max(1, static_cast<int>(expectation.threshold));
    if (owner_id <= 0 || completed_construction_by_owner.value(owner_id) < required) {
      add_issue(QStringLiteral("economy_no_construction"),
                QStringLiteral("%1 completed %2 of %3 required AI constructions")
                    .arg(expectation.group)
                    .arg(completed_construction_by_owner.value(owner_id))
                    .arg(required));
    }
    break;
  }
  case ArenaExpectationKind::OwnerHarvestsResource: {
    int owner_id = 0;
    if (!ids(expectation.group).empty()) {
      auto* anchor = world.get_entity(ids(expectation.group).front());
      auto const* unit = anchor != nullptr
                             ? anchor->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      owner_id = unit != nullptr ? unit->owner_id : 0;
    }
    const int required = std::max(1, static_cast<int>(expectation.threshold));
    if (owner_id <= 0 || completed_harvest_by_owner.value(owner_id) < required) {
      add_issue(QStringLiteral("economy_no_harvest"),
                QStringLiteral("%1 completed %2 of %3 required resource harvests")
                    .arg(expectation.group)
                    .arg(completed_harvest_by_owner.value(owner_id))
                    .arg(required));
    }
    break;
  }
  default:
    break;
  }
}

void ArenaScenarioRunner::Impl::check_frame_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::FrameBudget: {
    std::vector<double> samples;
    samples.reserve(trace.size());
    std::uint64_t peak_visible_soldiers = 0U;
    std::uint64_t peak_draw_commands = 0U;
    std::uint64_t peak_rigged_commands = 0U;
    std::uint64_t peak_rigged_instanced_instances = 0U;
    std::uint64_t peak_rigged_single_draws = 0U;
    std::uint64_t peak_shadow_rigged_instanced_instances = 0U;
    std::uint64_t peak_shadow_rigged_single_draws = 0U;
    std::uint64_t prewarm_frames = 0U;
    double prewarm_max_ms = 0.0;
    std::uint64_t gpu_timed_frames = 0U;
    for (auto const& frame : trace) {
      bool const after_start =
          frame.time_seconds + 1.0e-5F >= expectation.start_seconds;
      bool const before_end = expectation.end_seconds <= 0.0F ||
                              frame.time_seconds <= expectation.end_seconds + 1.0e-5F;
      if (frame.time_seconds < k_arena_prewarm_seconds) {

        ++prewarm_frames;
        prewarm_max_ms = std::max(prewarm_max_ms, frame.frame_time_ms);
        continue;
      }
      if (frame.timings.gpu_color_ms > 0.0 || frame.timings.gpu_shadow_ms > 0.0) {
        ++gpu_timed_frames;
      }
      if (after_start && before_end) {
        samples.push_back(frame.frame_time_ms);
        peak_visible_soldiers =
            std::max(peak_visible_soldiers, frame.timings.visible_soldiers);
        peak_draw_commands = std::max(peak_draw_commands, frame.timings.draw_calls);
        peak_rigged_commands =
            std::max(peak_rigged_commands, frame.timings.rigged_commands);
        peak_rigged_instanced_instances = std::max(
            peak_rigged_instanced_instances, frame.timings.rigged_instanced_instances);
        peak_rigged_single_draws =
            std::max(peak_rigged_single_draws, frame.timings.rigged_single_draws);
        peak_shadow_rigged_instanced_instances =
            std::max(peak_shadow_rigged_instanced_instances,
                     frame.timings.shadow_rigged_instanced_instances);
        peak_shadow_rigged_single_draws = std::max(
            peak_shadow_rigged_single_draws, frame.timings.shadow_rigged_single_draws);
      }
    }
    if (samples.empty()) {
      break;
    }
    std::sort(samples.begin(), samples.end());
    double const budget = expectation.threshold > 0.0F ? expectation.threshold
                                                       : k_default_frame_budget_ms;
    std::size_t const p95_index = std::min<std::size_t>(
        samples.size() - 1U, ((samples.size() * 95U) + 99U) / 100U - 1U);
    std::size_t const p50_index = std::min<std::size_t>(
        samples.size() - 1U, ((samples.size() * 50U) + 99U) / 100U - 1U);
    std::size_t const p99_index = std::min<std::size_t>(
        samples.size() - 1U, ((samples.size() * 99U) + 99U) / 100U - 1U);
    double const p50 = samples[p50_index];
    double const p95 = samples[p95_index];
    double const p99 = samples[p99_index];
    double const maximum = samples.back();

    report.frame_time_samples = samples.size();
    report.frame_budget_ms = budget;
    report.frame_time_p50_ms = p50;
    report.frame_time_p95_ms = p95;
    report.frame_time_p99_ms = p99;
    report.frame_time_max_ms = maximum;
    report.prewarm_frames = prewarm_frames;
    report.prewarm_max_ms = prewarm_max_ms;
    report.prewarm_seconds = k_arena_prewarm_seconds;
    report.gpu_timed_frames = gpu_timed_frames;

    auto percentile_of = [this](auto&& pick) -> double {
      std::vector<double> values;
      values.reserve(trace.size());
      for (auto const& frame : this->trace) {
        if (frame.time_seconds < k_arena_prewarm_seconds || !frame.commander.valid) {
          continue;
        }
        values.push_back(static_cast<double>(pick(frame.commander.costs)));
      }
      if (values.empty()) {
        return 0.0;
      }
      std::sort(values.begin(), values.end());
      std::size_t const index = std::min<std::size_t>(
          values.size() - 1U, ((values.size() * 95U) + 99U) / 100U - 1U);
      return values[index];
    };
    report.rpg_cost_p95_motor_ms =
        percentile_of([](auto const& costs) { return costs.motor_ms; });
    report.rpg_cost_p95_targeting_ms =
        percentile_of([](auto const& costs) { return costs.targeting_ms; });
    report.rpg_cost_p95_weapon_trace_ms =
        percentile_of([](auto const& costs) { return costs.weapon_trace_ms; });
    report.rpg_cost_p95_engagement_ms =
        percentile_of([](auto const& costs) { return costs.engagement_ms; });
    report.rpg_cost_p95_camera_ms =
        percentile_of([](auto const& costs) { return costs.camera_ms; });
    report.rpg_cost_p95_total_ms =
        percentile_of([](auto const& costs) { return costs.total_ms(); });

    std::vector<double> simulation_samples;
    simulation_samples.reserve(trace.size());
    for (auto const& frame : trace) {
      if (frame.time_seconds < k_arena_prewarm_seconds) {
        continue;
      }
      simulation_samples.push_back(frame.timings.simulation_ms);
    }
    if (!simulation_samples.empty()) {
      std::sort(simulation_samples.begin(), simulation_samples.end());
      std::size_t const simulation_index =
          std::min<std::size_t>(simulation_samples.size() - 1U,
                                ((simulation_samples.size() * 95U) + 99U) / 100U - 1U);
      report.simulation_p95_ms = simulation_samples[simulation_index];
    }
    report.peak_visible_soldiers = peak_visible_soldiers;
    report.peak_draw_commands = peak_draw_commands;
    report.peak_rigged_commands = peak_rigged_commands;
    report.peak_rigged_instanced_instances = peak_rigged_instanced_instances;
    report.peak_rigged_single_draws = peak_rigged_single_draws;
    report.peak_shadow_rigged_instanced_instances =
        peak_shadow_rigged_instanced_instances;
    report.peak_shadow_rigged_single_draws = peak_shadow_rigged_single_draws;

    bool const contains_troop_groups =
        std::any_of(scenario.groups.begin(),
                    scenario.groups.end(),
                    [](auto const& group) { return !group.spawn_type.has_value(); });
    if (contains_troop_groups && peak_visible_soldiers == 0U &&
        peak_rigged_commands == 0U) {
      add_issue(QStringLiteral("performance_scene_rendered_no_creatures"),
                QStringLiteral("performance sampling observed no visible creatures"));
    }
    if (scenario.require_rigged_instancing && peak_rigged_instanced_instances == 0U) {
      add_issue(QStringLiteral("performance_rigged_instancing_unused"),
                QStringLiteral("performance sampling observed no instanced "
                               "rigged creature playback"));
    }

    if (p95 > budget || maximum > budget * 10.0) {
      add_issue(
          QStringLiteral("frame_budget_exceeded"),
          QStringLiteral("render frame p95/p99/max was %1/%2/%3 ms (budget %4 ms)")
              .arg(p95, 0, 'f', 2)
              .arg(p99, 0, 'f', 2)
              .arg(maximum, 0, 'f', 2)
              .arg(budget, 0, 'f', 2));
    }
    if (gpu_timed_frames == 0U) {
      add_issue(QStringLiteral("performance_gpu_timing_missing"),
                QStringLiteral("no post-prewarm frame reported a GPU timing, so "
                               "the frame budget cannot be attributed"));
    }
    break;
  }
  default:
    break;
  }
}

void ArenaScenarioRunner::Impl::check_world_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::UndeadZoneDormantBefore: {
    auto const state = undead_zone_state(expectation.zone_id);
    float const window = std::max(expectation.end_seconds, expectation.threshold);
    if (state.first_spawn_at >= 0.0F && state.first_spawn_at < window) {
      add_issue(QStringLiteral("undead_zone_woke_too_early"),
                QStringLiteral("%1 spawned guardians at %2 s but had to stay "
                               "dormant for %3 s")
                    .arg(expectation.zone_id)
                    .arg(state.first_spawn_at, 0, 'f', 2)
                    .arg(window, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::UndeadZoneAwakened: {
    auto const state = undead_zone_state(expectation.zone_id);
    int const required = std::max(1, static_cast<int>(expectation.threshold));
    if (state.spawned_total < required) {
      add_issue(QStringLiteral("undead_zone_never_awakened"),
                QStringLiteral("%1 spawned %2 guardian(s); expected at least %3")
                    .arg(expectation.zone_id)
                    .arg(state.spawned_total)
                    .arg(required));
    }
    break;
  }
  case ArenaExpectationKind::UndeadZoneCleared: {
    auto const state = undead_zone_state(expectation.zone_id);
    if (state.spawned_total == 0) {
      add_issue(QStringLiteral("undead_zone_never_awakened"),
                QStringLiteral("%1 never spawned guardians, so it cannot be cleared")
                    .arg(expectation.zone_id));
    } else if (state.alive > 0) {
      add_issue(QStringLiteral("undead_zone_not_cleared"),
                QStringLiteral("%1 still holds %2 living guardian(s)")
                    .arg(expectation.zone_id)
                    .arg(state.alive));
    }
    break;
  }
  case ArenaExpectationKind::UndeadZoneShrineStands: {
    auto const state = undead_zone_state(expectation.zone_id);
    if (!state.shrine_seen) {
      add_issue(
          QStringLiteral("undead_zone_shrine_missing"),
          QStringLiteral("%1 never raised a magic shrine").arg(expectation.zone_id));
    } else if (!state.shrine_standing) {
      add_issue(QStringLiteral("undead_zone_shrine_lost"),
                QStringLiteral("%1 lost the shrine it was meant to keep")
                    .arg(expectation.zone_id));
    }
    break;
  }
  case ArenaExpectationKind::UndeadZoneShrineDestroyed: {
    auto const state = undead_zone_state(expectation.zone_id);
    if (!state.shrine_seen) {
      add_issue(QStringLiteral("undead_zone_shrine_missing"),
                QStringLiteral("%1 never raised a magic shrine to destroy")
                    .arg(expectation.zone_id));
    } else if (!state.shrine_destroyed) {
      add_issue(QStringLiteral("undead_zone_shrine_survived"),
                QStringLiteral("%1 still holds its shrine").arg(expectation.zone_id));
    }
    break;
  }
  case ArenaExpectationKind::WildlifeGrazingObserved:
    if (!wildlife_observation.grazing_seen) {
      add_issue(QStringLiteral("wildlife_never_grazed"),
                QStringLiteral("no animal settled into grazing during the run"));
    }
    break;
  case ArenaExpectationKind::WildlifeFleeObserved:
    if (!wildlife_observation.flee_seen) {
      add_issue(QStringLiteral("wildlife_never_fled"),
                QStringLiteral("no animal fled or scattered during the run"));
    }
    break;
  case ArenaExpectationKind::WildlifeHuntObserved:
    if (!wildlife_observation.hunt_seen) {
      add_issue(QStringLiteral("wildlife_never_hunted"),
                QStringLiteral("no wolf started stalking prey during the run"));
    }
    break;
  case ArenaExpectationKind::WildlifeBirdsScattered:
    if (wildlife_observation.bird_scatter_events == 0U) {
      add_issue(QStringLiteral("bird_flock_never_scattered"),
                QStringLiteral("the flock never scattered from a threat"));
    }
    break;
  case ArenaExpectationKind::WildlifeBirdFlyoverObserved:
    if (wildlife_observation.bird_flyovers == 0U) {
      add_issue(QStringLiteral("bird_flyover_never_launched"),
                QStringLiteral("no flock crossed the sky during the run"));
    }
    break;
  case ArenaExpectationKind::WildlifePopulationHeld: {
    int const required = std::max(1, static_cast<int>(expectation.threshold));
    if (wildlife_observation.min_population < required) {
      add_issue(QStringLiteral("wildlife_population_dropped"),
                QStringLiteral("wildlife population fell to %1, below the "
                               "required %2 (peak %3)")
                    .arg(wildlife_observation.min_population)
                    .arg(required)
                    .arg(wildlife_observation.peak_population));
    }
    break;
  }
  case ArenaExpectationKind::WildlifeCasualtyObserved:
    if (wildlife_observation.min_population >= wildlife_observation.peak_population) {
      add_issue(QStringLiteral("wildlife_never_culled"),
                QStringLiteral("no animal was killed during the run (population "
                               "held at %1)")
                    .arg(wildlife_observation.peak_population));
    }
    break;
  case ArenaExpectationKind::UnitsClearOfBuildings:
    if (building_overlap_report.contains(expectation.group)) {
      add_issue(QStringLiteral("unit_inside_building"),
                QStringLiteral("%1 walked through a building: %2")
                    .arg(expectation.group,
                         building_overlap_report.value(expectation.group)));
    }
    break;
  case ArenaExpectationKind::NoRenderVisibilityChurn:
  case ArenaExpectationKind::RpgFormationSurvivesLensGap:
  case ArenaExpectationKind::FullCreatureDetailOnly:
  case ArenaExpectationKind::NoFullscreenFlash:

    break;
  default:
    break;
  }
}

} // namespace Arena
