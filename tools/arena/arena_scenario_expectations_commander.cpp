#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

constexpr float k_boom_reversal_floor = 0.005F;

constexpr float k_presented_yaw_allowance_degrees = 0.5F;

} // namespace

void ArenaScenarioRunner::Impl::check_commander_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::CommanderPresentedPoseAgrees: {
    float const allowed = expectation.threshold > 0.0F ? expectation.threshold : 0.01F;
    auto const frames = commander_frames();
    float worst = 0.0F;
    float worst_time = 0.0F;
    float worst_yaw = 0.0F;
    float worst_yaw_time = 0.0F;
    int compared = 0;
    for (auto const* frame : frames) {
      auto const& shot = frame->commander.camera;
      if (!shot.valid || frame->soldiers.empty()) {
        continue;
      }
      ++compared;
      float const gap = horizontal_distance(shot.commander_position,
                                            frame->soldiers[0].root_position);
      if (gap > worst) {
        worst = gap;
        worst_time = frame->time_seconds;
      }

      float const yaw_error = std::abs(shortest_degrees(
          frame->commander.motor.presented_yaw, frame->soldiers[0].root_yaw_degrees));
      if (yaw_error > worst_yaw) {
        worst_yaw = yaw_error;
        worst_yaw_time = frame->time_seconds;
      }
    }
    if (compared == 0) {
      add_issue(QStringLiteral("commander_pose_not_compared"),
                QStringLiteral("%1 never had a camera and a rendered body in the "
                               "same frame")
                    .arg(expectation.group));
      break;
    }
    if (worst > allowed) {
      add_issue(QStringLiteral("commander_presented_pose_disagreement"),
                QStringLiteral("the camera framed a point %1 m from the body it "
                               "was drawing at %2 s (allowed %3 m)")
                    .arg(worst, 0, 'f', 4)
                    .arg(worst_time, 0, 'f', 2)
                    .arg(allowed, 0, 'f', 4));
    }
    if (worst_yaw > k_presented_yaw_allowance_degrees) {
      add_issue(QStringLiteral("commander_presented_yaw_disagreement"),
                QStringLiteral("the presented pose faced %1 degrees away from the "
                               "body that was drawn at %2 s (allowed %3)")
                    .arg(worst_yaw, 0, 'f', 3)
                    .arg(worst_yaw_time, 0, 'f', 2)
                    .arg(k_presented_yaw_allowance_degrees, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::CommanderCameraClearanceAtLeast: {
    float const required = expectation.threshold > 0.0F ? expectation.threshold : 0.10F;
    auto const frames = commander_frames();
    float worst = std::numeric_limits<float>::max();
    float worst_time = 0.0F;
    int sampled = 0;
    for (auto const* frame : frames) {
      auto const& shot = frame->commander.camera;
      if (!shot.valid) {
        continue;
      }
      ++sampled;
      if (shot.eye_clearance < worst) {
        worst = shot.eye_clearance;
        worst_time = frame->time_seconds;
      }
    }
    if (sampled == 0) {
      add_issue(
          QStringLiteral("commander_camera_not_traced"),
          QStringLiteral("%1 never published a camera trace").arg(expectation.group));
      break;
    }
    if (worst < 0.0F) {
      add_issue(QStringLiteral("commander_camera_penetrated"),
                QStringLiteral("camera eye was %1 m inside an obstacle at %2 s")
                    .arg(-worst, 0, 'f', 3)
                    .arg(worst_time, 0, 'f', 2));
    } else if (worst < required) {
      add_issue(QStringLiteral("commander_camera_clearance"),
                QStringLiteral("camera eye came within %1 m of an obstacle at %2 s "
                               "(needs %3 m)")
                    .arg(worst, 0, 'f', 3)
                    .arg(worst_time, 0, 'f', 2)
                    .arg(required, 0, 'f', 3));
    }
    break;
  }
  case ArenaExpectationKind::CommanderCameraKeepsCommanderInSight: {

    float const allowed = expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
    auto const frames = commander_frames();
    float blocked_run = 0.0F;
    float worst_run = 0.0F;
    float worst_run_end = 0.0F;
    int sampled = 0;
    for (auto const* frame : frames) {
      auto const& shot = frame->commander.camera;
      if (!shot.valid) {
        continue;
      }
      ++sampled;
      if (shot.sight_line_clear) {
        blocked_run = 0.0F;
        continue;
      }
      blocked_run += std::max(shot.dt, 0.0F);
      if (blocked_run > worst_run) {
        worst_run = blocked_run;
        worst_run_end = frame->time_seconds;
      }
    }
    if (sampled == 0) {
      add_issue(
          QStringLiteral("commander_camera_not_traced"),
          QStringLiteral("%1 never published a camera trace").arg(expectation.group));
      break;
    }
    if (worst_run > allowed) {
      add_issue(QStringLiteral("commander_camera_lost_sight"),
                QStringLiteral("geometry stood between the lens and the commander for "
                               "%1 s, ending at %2 s (allowed %3 s); the boom has to "
                               "shorten until he is in sight and recover on its own")
                    .arg(worst_run, 0, 'f', 3)
                    .arg(worst_run_end, 0, 'f', 2)
                    .arg(allowed, 0, 'f', 3));
    }
    break;
  }
  case ArenaExpectationKind::CommanderBoomIsContinuous: {
    float const budget = expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
    auto const frames = commander_frames();
    float previous_boom = 0.0F;
    float previous_step = 0.0F;
    bool have_previous = false;
    int reversals = 0;
    for (auto const* frame : frames) {
      auto const& shot = frame->commander.camera;
      if (!shot.valid) {
        continue;
      }
      if (have_previous) {
        float const step = shot.boom_resolved - previous_boom;

        float const allowed = budget * std::max(1.0F, shot.dt * 60.0F);
        if (step > allowed) {
          add_issue(QStringLiteral("commander_boom_discontinuity"),
                    QStringLiteral("camera boom extended %1 m in one frame at %2 s "
                                   "(allowed %3 m); retraction may be immediate "
                                   "but release has to be damped")
                        .arg(step, 0, 'f', 3)
                        .arg(frame->time_seconds, 0, 'f', 2)
                        .arg(allowed, 0, 'f', 3));
          break;
        }
        if (std::abs(step) > k_boom_reversal_floor &&
            std::abs(previous_step) > k_boom_reversal_floor &&
            ((step > 0.0F) != (previous_step > 0.0F)) &&
            shot.boom_clear_fraction < 1.0F) {
          ++reversals;
        }
        previous_step = step;
      }
      previous_boom = shot.boom_resolved;
      have_previous = true;
    }
    int const allowed_reversals =
        std::max(2, static_cast<int>(std::lround(expectation.distance)));
    if (reversals > allowed_reversals) {
      add_issue(QStringLiteral("commander_boom_pumping"),
                QStringLiteral("camera boom reversed direction %1 times while an "
                               "obstruction stayed active (allowed %2)")
                    .arg(reversals)
                    .arg(allowed_reversals));
    }
    break;
  }
  case ArenaExpectationKind::NoUncommandedViewRotation: {
    float const allowed = expectation.threshold > 0.0F ? expectation.threshold : 0.05F;
    for (auto const* frame : commander_frames()) {
      auto const& shot = frame->commander.camera;
      auto const& in = frame->commander.input;
      if (!shot.valid || shot.framing_changed ||
          frame->commander.combat.locked_target_id != 0) {
        continue;
      }
      if (std::abs(in.look_delta_yaw) > 1.0e-4F ||
          std::abs(in.look_delta_pitch) > 1.0e-4F) {
        continue;
      }
      float const yaw_step = std::abs(shot.yaw_velocity * shot.dt);
      float const pitch_step = std::abs(shot.pitch_velocity * shot.dt);
      if (yaw_step > allowed || pitch_step > allowed) {
        add_issue(
            QStringLiteral("commander_view_rotated_uncommanded"),
            QStringLiteral("view turned %1 deg yaw / %2 deg pitch at %3 s with no "
                           "look input, no lock and no framing change (allowed %4)")
                .arg(yaw_step, 0, 'f', 3)
                .arg(pitch_step, 0, 'f', 3)
                .arg(frame->time_seconds, 0, 'f', 2)
                .arg(allowed, 0, 'f', 3));
        break;
      }
    }
    break;
  }
  case ArenaExpectationKind::CommanderMotorCorrectionWithin: {
    float const budget = expectation.threshold > 0.0F ? expectation.threshold : 0.08F;
    for (auto const* frame : commander_frames()) {
      auto const& motor = frame->commander.motor;
      float const correction =
          std::max(motor.snap_back_distance, motor.separation_push);

      float const allowed = budget * std::max(1.0F, motor.dt * 60.0F);
      if (correction > allowed) {
        add_issue(
            QStringLiteral("commander_motor_correction"),
            QStringLiteral("motor corrected the body %1 m in one tick at %2 s via "
                           "%3 (allowed %4 m)")
                .arg(correction, 0, 'f', 3)
                .arg(frame->time_seconds, 0, 'f', 2)
                .arg(QString::fromLatin1(
                    App::Core::displacement_source_name(motor.displacement_source)))
                .arg(allowed, 0, 'f', 3));
        break;
      }
    }
    break;
  }
  case ArenaExpectationKind::CommanderSpeedIsContinuous: {
    float const allowed = expectation.threshold > 0.0F ? expectation.threshold : 4.0F;
    float previous_speed = 0.0F;
    bool have_previous = false;
    for (auto const* frame : commander_frames()) {
      auto const& motor = frame->commander.motor;
      float const speed = motor.actual_velocity.length();
      if (have_previous && motor.dt > 0.0F) {
        float const change = std::abs(speed - previous_speed) / motor.dt;
        if (change > allowed) {
          add_issue(QStringLiteral("commander_speed_discontinuity"),
                    QStringLiteral("planar speed changed %1 m/s^2 at %2 s via %3 "
                                   "(allowed %4 m/s^2)")
                        .arg(change, 0, 'f', 2)
                        .arg(frame->time_seconds, 0, 'f', 2)
                        .arg(QString::fromLatin1(App::Core::displacement_source_name(
                            motor.displacement_source)))
                        .arg(allowed, 0, 'f', 2));
          break;
        }
      }
      previous_speed = speed;
      have_previous = true;
    }
    break;
  }
  case ArenaExpectationKind::CommanderInputEdgesAllConsumed: {
    auto const frames = commander_frames();
    if (frames.empty()) {
      add_issue(QStringLiteral("commander_input_not_traced"),
                QStringLiteral("no commander presentation trace was recorded, so "
                               "input edges cannot be accounted for"));
      break;
    }
    auto const& last = frames.back()->commander.input;
    if (last.primary_press_sequence !=
        last.primary_consumed_sequence + last.primary_dropped_sequence) {
      add_issue(QStringLiteral("commander_attack_edge_unaccounted"),
                QStringLiteral("%1 attack presses produced %2 consumed and %3 dropped; "
                               "every edge must land in exactly one of the two")
                    .arg(last.primary_press_sequence)
                    .arg(last.primary_consumed_sequence)
                    .arg(last.primary_dropped_sequence));
    }
    auto const allowed_drops =
        static_cast<std::uint64_t>(std::max(0.0F, expectation.threshold));
    if (last.primary_dropped_sequence > allowed_drops) {
      add_issue(QStringLiteral("commander_attack_edge_dropped"),
                QStringLiteral("%1 attack presses were dropped without reaching "
                               "the simulation (allowed %2)")
                    .arg(last.primary_dropped_sequence)
                    .arg(allowed_drops));
    }
    if (last.dodge_request_sequence !=
        last.dodge_consumed_sequence + last.dodge_refused_sequence) {
      add_issue(QStringLiteral("commander_dodge_edge_unaccounted"),
                QStringLiteral("%1 dodge requests produced %2 consumed and %3 refused")
                    .arg(last.dodge_request_sequence)
                    .arg(last.dodge_consumed_sequence)
                    .arg(last.dodge_refused_sequence));
    }
    break;
  }
  case ArenaExpectationKind::CommanderCombatCounterWithin: {
    auto const frames = commander_frames();
    if (frames.empty()) {
      add_issue(QStringLiteral("commander_combat_not_traced"),
                QStringLiteral("no commander presentation trace was recorded, so "
                               "%1 cannot be counted")
                    .arg(expectation.counter_key));
      break;
    }
    auto const counter_of =
        [&expectation](const App::Core::CommanderCombatTrace& combat) -> std::uint32_t {
      auto const& key = expectation.counter_key;
      if (key == QLatin1String("accepted")) {
        return combat.queue_accepted;
      }
      if (key == QLatin1String("buffered")) {
        return combat.queue_buffered;
      }
      if (key == QLatin1String("refused")) {
        return combat.queue_refused;
      }
      if (key == QLatin1String("expired")) {
        return combat.queue_expired;
      }
      if (key == QLatin1String("overflow")) {
        return combat.queue_overflow;
      }
      if (key == QLatin1String("block")) {
        return combat.blocked_contacts;
      }
      if (key == QLatin1String("perfect_guard")) {
        return combat.perfect_guard_contacts;
      }
      if (key == QLatin1String("dodge")) {
        return combat.dodged_contacts;
      }
      if (key == QLatin1String("damage")) {
        return combat.damaging_contacts;
      }
      if (key == QLatin1String("guard_break")) {
        return combat.guard_broken_contacts;
      }
      return 0U;
    };

    float const window_start = expectation.start_seconds;
    float const window_end = expectation.end_seconds > 0.0F
                                 ? expectation.end_seconds
                                 : std::numeric_limits<float>::max();
    std::optional<std::uint32_t> first;
    std::uint32_t last_value = 0U;
    bool have_last = false;
    for (auto const* frame : frames) {
      if (frame->time_seconds < window_start) {
        first = counter_of(frame->commander.combat);
        continue;
      }
      if (frame->time_seconds > window_end) {
        break;
      }
      if (!first.has_value()) {
        first = counter_of(frame->commander.combat);
      }
      last_value = counter_of(frame->commander.combat);
      have_last = true;
    }
    if (!have_last) {
      add_issue(QStringLiteral("commander_combat_counter_window_empty"),
                QStringLiteral("no traced frame fell inside %1 s - %2 s for %3")
                    .arg(window_start, 0, 'f', 2)
                    .arg(expectation.end_seconds, 0, 'f', 2)
                    .arg(expectation.counter_key));
      break;
    }
    auto const observed = last_value - first.value_or(0U);
    auto const minimum =
        static_cast<std::uint32_t>(std::max(0.0F, expectation.threshold));
    if (observed < minimum) {
      add_issue(QStringLiteral("commander_combat_counter_too_low"),
                QStringLiteral("%1 was counted %2 times between %3 s and %4 s but "
                               "at least %5 were required")
                    .arg(expectation.counter_key)
                    .arg(observed)
                    .arg(window_start, 0, 'f', 2)
                    .arg(window_end, 0, 'f', 2)
                    .arg(minimum));
      break;
    }
    if (expectation.maximum >= 0.0F) {
      auto const maximum = static_cast<std::uint32_t>(expectation.maximum);
      if (observed > maximum) {
        add_issue(QStringLiteral("commander_combat_counter_too_high"),
                  QStringLiteral("%1 was counted %2 times between %3 s and %4 s "
                                 "but at most %5 are allowed")
                      .arg(expectation.counter_key)
                      .arg(observed)
                      .arg(window_start, 0, 'f', 2)
                      .arg(window_end, 0, 'f', 2)
                      .arg(maximum));
      }
    }
    break;
  }
  case ArenaExpectationKind::CommanderLockStateWithin: {
    auto const frames = commander_frames();
    if (frames.empty()) {
      add_issue(QStringLiteral("commander_lock_not_traced"),
                QStringLiteral("no commander presentation trace was recorded, so "
                               "the lock state cannot be read"));
      break;
    }
    float const window_start = expectation.start_seconds;
    float const window_end = expectation.end_seconds > 0.0F
                                 ? expectation.end_seconds
                                 : std::numeric_limits<float>::max();
    std::vector<std::uint64_t> locks;
    for (auto const* frame : frames) {
      if (frame->time_seconds < window_start || frame->time_seconds > window_end) {
        continue;
      }
      locks.push_back(frame->commander.combat.locked_target_id);
    }
    if (locks.empty()) {
      add_issue(QStringLiteral("commander_lock_window_empty"),
                QStringLiteral("no traced frame fell inside %1 s - %2 s")
                    .arg(window_start, 0, 'f', 2)
                    .arg(expectation.end_seconds, 0, 'f', 2));
      break;
    }
    auto const& key = expectation.counter_key;
    if (key == QLatin1String("held")) {
      auto const lost = std::find(locks.begin(), locks.end(), 0U);
      if (lost != locks.end()) {
        add_issue(QStringLiteral("commander_lock_not_held"),
                  QStringLiteral("the lock was empty inside %1 s - %2 s")
                      .arg(window_start, 0, 'f', 2)
                      .arg(window_end, 0, 'f', 2));
      }
    } else if (key == QLatin1String("cleared")) {
      auto const still_locked = std::find_if(
          locks.begin(), locks.end(), [](std::uint64_t id) { return id != 0U; });
      if (still_locked != locks.end()) {
        add_issue(QStringLiteral("commander_lock_not_cleared"),
                  QStringLiteral("entity %1 was still locked inside %2 s - %3 s")
                      .arg(*still_locked)
                      .arg(window_start, 0, 'f', 2)
                      .arg(window_end, 0, 'f', 2));
      }
    } else if (key == QLatin1String("changed")) {
      if (locks.front() == locks.back()) {
        add_issue(QStringLiteral("commander_lock_did_not_change"),
                  QStringLiteral("the lock stayed on entity %1 across %2 s - %3 s")
                      .arg(locks.front())
                      .arg(window_start, 0, 'f', 2)
                      .arg(window_end, 0, 'f', 2));
      }
    }
    break;
  }
  case ArenaExpectationKind::CommanderContactCountAtMost: {
    int const allowed_floor =
        std::max(1,
                 static_cast<int>(std::lround(
                     expectation.threshold > 0.0F ? expectation.threshold : 1.0F)));
    for (auto const* frame : commander_frames()) {
      auto const& combat = frame->commander.combat;
      int allowed = allowed_floor;
      for (auto const& unit : frame->units) {
        if (unit.group != expectation.group) {
          continue;
        }
        auto const* definition =
            Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    unit.combat_action_id));
        if (definition != nullptr) {
          allowed = std::max(allowed, definition->max_targets);
        }
        break;
      }
      if (combat.action_running && combat.action_hit_count > allowed) {
        add_issue(QStringLiteral("commander_contact_multiplicity"),
                  QStringLiteral("one action landed %1 contacts by %2 s, but at "
                                 "most %3 is authored")
                      .arg(combat.action_hit_count)
                      .arg(frame->time_seconds, 0, 'f', 2)
                      .arg(allowed));
        break;
      }
    }
    break;
  }
  case ArenaExpectationKind::CommanderActionObserved: {
    auto const frames = commander_frames();
    if (frames.empty()) {
      add_issue(QStringLiteral("commander_action_not_traced"),
                QStringLiteral("no commander presentation trace was recorded, so "
                               "action %1 cannot be verified")
                    .arg(expectation.combat_action_id));
      break;
    }
    float const window_start = expectation.start_seconds;
    float const window_end = expectation.end_seconds > 0.0F
                                 ? expectation.end_seconds
                                 : std::numeric_limits<float>::max();
    bool sampled_window = false;
    bool observed = false;
    for (auto const* frame : frames) {
      if (frame->time_seconds < window_start || frame->time_seconds > window_end) {
        continue;
      }
      sampled_window = true;
      if (frame->commander.combat.action_running &&
          frame->commander.combat.action_id == expectation.combat_action_id) {
        observed = true;
        break;
      }
    }
    if (!sampled_window) {
      add_issue(QStringLiteral("commander_action_window_empty"),
                QStringLiteral("no traced frame fell inside %1 s - %2 s for "
                               "action %3")
                    .arg(window_start, 0, 'f', 2)
                    .arg(expectation.end_seconds, 0, 'f', 2)
                    .arg(expectation.combat_action_id));
    } else if (!observed) {
      add_issue(QStringLiteral("commander_action_not_observed"),
                QStringLiteral("%1 never ran authored action %2 between %3 s and "
                               "%4 s")
                    .arg(expectation.group)
                    .arg(expectation.combat_action_id)
                    .arg(window_start, 0, 'f', 2)
                    .arg(expectation.end_seconds, 0, 'f', 2));
    }
    break;
  }
  default:
    break;
  }
}

void ArenaScenarioRunner::Impl::check_rpg_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::CommanderAuraActivated:
    if (!commander_aura_active_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("commander_aura_not_activated"),
                QStringLiteral("%1 never entered its timed command aura state")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::CommanderAuraBuffObserved:
    if (!commander_aura_buff_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("commander_aura_buff_not_observed"),
                QStringLiteral("%1 never received the nearby commander bonus")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::CommanderAuraExpired:
    if (!commander_aura_expired_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("commander_aura_not_expired"),
                QStringLiteral("%1 command aura did not expire into cooldown")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::NoCommanderAuraBuffObserved:
    if (commander_aura_buff_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("commander_aura_leaked_outside_radius"),
                QStringLiteral("%1 received a commander bonus outside the aura")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::ExactRpgTargetObserved:
    if (!exact_rpg_target_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("exact_rpg_target_not_observed"),
                QStringLiteral("%1 never selected an exact in-range soldier")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgDamageContactObserved:
    if (!rpg_damage_contact_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("rpg_damage_contact_not_observed"),
                QStringLiteral("%1 never published a visible RPG damage contact")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgBlockContactObserved:
    if (!rpg_block_contact_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("rpg_block_contact_not_observed"),
                QStringLiteral("%1 never published a visible block contact")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgDodgeContactObserved:
    if (!rpg_dodge_contact_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("rpg_dodge_contact_not_observed"),
                QStringLiteral("%1 never published a visible dodge contact")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgDodgeWindowObserved:
    if (!rpg_dodge_window_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("rpg_dodge_window_not_observed"),
                QStringLiteral("%1 never entered its RPG dodge protection window")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgHealthReduced:
    if (!initial_rpg_health_by_group.contains(expectation.group) ||
        minimum_rpg_health_by_group.value(expectation.group) >=
            initial_rpg_health_by_group.value(expectation.group)) {
      add_issue(QStringLiteral("rpg_health_not_reduced"),
                QStringLiteral("%1 took no RPG health damage").arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgHealthUnchanged:
    if (!initial_rpg_health_by_group.contains(expectation.group) ||
        minimum_rpg_health_by_group.value(expectation.group) !=
            initial_rpg_health_by_group.value(expectation.group)) {
      add_issue(QStringLiteral("rpg_health_changed"),
                QStringLiteral("%1 lost RPG health during a protected scenario")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgWalkObserved:
    if (!rpg_walk_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("rpg_walk_not_observed"),
                QStringLiteral("%1 never entered a walking locomotion state")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgRunObserved:
    if (!rpg_run_seen.value(expectation.group, false)) {
      add_issue(QStringLiteral("rpg_run_not_observed"),
                QStringLiteral("%1 never entered a running locomotion state")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RpgLocomotionAnimationMatched:
    if (auto const mismatch = rpg_locomotion_mismatch.constFind(expectation.group);
        mismatch != rpg_locomotion_mismatch.cend()) {
      add_issue(QStringLiteral("rpg_locomotion_desynchronized"),
                QStringLiteral("%1 %2").arg(expectation.group, mismatch.value()));
    }
    break;
  case ArenaExpectationKind::RpgStrikeAnimationMatched:
    if (auto const mismatch = rpg_strike_mismatch.constFind(expectation.group);
        mismatch != rpg_strike_mismatch.cend()) {
      add_issue(QStringLiteral("rpg_strike_desynchronized"),
                QStringLiteral("%1 %2").arg(expectation.group, mismatch.value()));
    }
    break;
  case ArenaExpectationKind::RpgSwingCadenceWithin: {
    auto const& starts = rpg_swing_starts[expectation.group];
    int const required_swings =
        std::max(2, static_cast<int>(std::lround(expectation.distance)));
    float const allowed_gap =
        expectation.threshold > 0.0F ? expectation.threshold : 1.0F;
    if (static_cast<int>(starts.size()) < required_swings) {
      add_issue(QStringLiteral("rpg_swing_cadence_too_few"),
                QStringLiteral("%1 started %2 swings while holding the attack "
                               "input but needed %3")
                    .arg(expectation.group)
                    .arg(starts.size())
                    .arg(required_swings));
      break;
    }
    for (std::size_t i = 1; i < starts.size(); ++i) {
      float const gap = starts[i] - starts[i - 1U];
      if (gap > allowed_gap) {
        add_issue(QStringLiteral("rpg_swing_cadence_too_slow"),
                  QStringLiteral("%1 waited %2 s between swings at %3 s but the "
                                 "held attack has to chain within %4 s")
                      .arg(expectation.group)
                      .arg(gap, 0, 'f', 2)
                      .arg(starts[i], 0, 'f', 2)
                      .arg(allowed_gap, 0, 'f', 2));
        break;
      }
    }
    break;
  }
  case ArenaExpectationKind::RpgSwingCarriesBody: {
    auto const& carries = rpg_swing_carry[expectation.group];
    int const required_swings =
        std::max(1, static_cast<int>(std::lround(expectation.distance)));
    float const required_carry = std::max(expectation.threshold, 0.0F);
    if (static_cast<int>(carries.size()) < required_swings) {
      add_issue(QStringLiteral("rpg_swing_carry_too_few"),
                QStringLiteral("%1 swung %2 times but needed %3 to judge the "
                               "body carry")
                    .arg(expectation.group)
                    .arg(carries.size())
                    .arg(required_swings));
      break;
    }
    for (std::size_t i = 0; i < carries.size(); ++i) {
      if (carries[i] + 1.0e-3F < required_carry) {
        add_issue(QStringLiteral("rpg_swing_planted"),
                  QStringLiteral("%1 swing %2 carried the body %3 m but a strike "
                                 "has to drive it at least %4 m")
                      .arg(expectation.group)
                      .arg(i + 1U)
                      .arg(carries[i], 0, 'f', 2)
                      .arg(required_carry, 0, 'f', 2));
        break;
      }
    }
    break;
  }
  case ArenaExpectationKind::RpgTravelObserved: {
    auto const observation = rpg_travel_observations.value(travel_key(expectation));
    float const required = std::max(expectation.threshold, 0.0F);
    if (!observation.has_start || !observation.has_end) {
      add_issue(QStringLiteral("rpg_travel_not_sampled"),
                QStringLiteral("%1 was never sampled across its travel window")
                    .arg(expectation.group));
      break;
    }
    float const travelled = horizontal_distance(observation.start, observation.end);
    if (travelled < required) {
      add_issue(QStringLiteral("rpg_travel_blocked"),
                QStringLiteral("%1 travelled %2 m between %3 s and %4 s but had "
                               "to cover %5 m")
                    .arg(expectation.group)
                    .arg(travelled, 0, 'f', 2)
                    .arg(expectation.start_seconds, 0, 'f', 2)
                    .arg(expectation.end_seconds, 0, 'f', 2)
                    .arg(required, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::RpgApproachWithin: {
    float const required = expectation.distance > 0.0F ? expectation.distance : 1.0F;
    const QString key =
        projectile_pair_key(expectation.group, expectation.target_group);
    auto const closest = minimum_group_pair_distance.constFind(key);
    if (closest == minimum_group_pair_distance.cend()) {
      add_issue(QStringLiteral("rpg_approach_not_sampled"),
                QStringLiteral("%1 and %2 were never sampled together")
                    .arg(expectation.group, expectation.target_group));
    } else if (closest.value() > required) {
      add_issue(QStringLiteral("rpg_approach_blocked"),
                QStringLiteral("%1 closed to %2 m of %3 but had to reach %4 m")
                    .arg(expectation.group)
                    .arg(closest.value(), 0, 'f', 2)
                    .arg(expectation.target_group)
                    .arg(required, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::GroupPairKeepsApart: {
    const QString key =
        projectile_pair_key(expectation.group, expectation.target_group);
    auto const closest = minimum_group_pair_standoff.constFind(key);
    if (closest == minimum_group_pair_standoff.cend()) {
      add_issue(QStringLiteral("group_pair_not_sampled"),
                QStringLiteral("%1 and %2 were never sampled together")
                    .arg(expectation.group, expectation.target_group));
    } else if (closest.value() < expectation.distance) {
      add_issue(QStringLiteral("group_pair_interpenetrated"),
                QStringLiteral("%1 came within %2 m of %3 at %4 s but has to "
                               "keep %5 m apart")
                    .arg(expectation.group)
                    .arg(closest.value(), 0, 'f', 2)
                    .arg(expectation.target_group)
                    .arg(minimum_group_pair_standoff_at.value(key), 0, 'f', 2)
                    .arg(expectation.distance, 0, 'f', 2));
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Arena
