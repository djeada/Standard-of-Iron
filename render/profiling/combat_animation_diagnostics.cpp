#include "combat_animation_diagnostics.h"

#include <QDebug>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "animation/combat_manifest.h"
#include "render/creature/animation_core_bridge.h"

namespace Render::Profiling {

namespace {

constexpr float k_attack_phase_reset_epsilon = 0.02F;
constexpr float k_churn_window_seconds = 1.0F;
constexpr std::uint32_t k_churn_threshold = 5U;
constexpr std::uint32_t k_extreme_churn_threshold = 8U;
constexpr float k_rapid_cycle_seconds = 0.30F;

auto visual_state_from_sample(const SoldierAnimationDebugSample& sample) noexcept
    -> SoldierVisualState {
  if (sample.animation_state == Render::Creature::AnimationStateId::Dead) {
    return SoldierVisualState::Dead;
  }
  if (sample.animation_state == Render::Creature::AnimationStateId::Die) {
    return SoldierVisualState::Dying;
  }
  if (sample.is_hit_reacting) {
    return SoldierVisualState::HitReaction;
  }
  if (sample.is_attacking) {
    return SoldierVisualState::Attack;
  }
  switch (sample.locomotion_state) {
  case Render::Creature::MovementAnimationState::Run:
    return SoldierVisualState::Run;
  case Render::Creature::MovementAnimationState::Walk:
    return SoldierVisualState::Walk;
  case Render::Creature::MovementAnimationState::Idle:
    break;
  }
  return SoldierVisualState::Idle;
}

} // namespace

auto attack_phase_window(Render::GL::CombatAnimPhase phase,
                         bool amplified_attack,
                         bool finisher_attack) noexcept -> CombatPhaseWindow {
  auto const window =
      Animation::attack_phase_window(Render::Creature::animation_combat_phase(phase),
                                     amplified_attack,
                                     finisher_attack);
  return {window.start, window.end, window.offset_weight};
}

auto scrubbed_combat_phase_from_attack_phase(float attack_phase,
                                             bool amplified_attack,
                                             bool finisher_attack) noexcept
    -> ScrubbedCombatPhase {
  auto const scrubbed = Animation::scrubbed_combat_phase_from_attack_phase(
      attack_phase, amplified_attack, finisher_attack);
  return {Render::Creature::engine_combat_phase(scrubbed.phase), scrubbed.progress};
}

auto combat_phase_name(Render::GL::CombatAnimPhase phase) noexcept -> const char* {
  switch (phase) {
  case Render::GL::CombatAnimPhase::Idle:
    return "Idle";
  case Render::GL::CombatAnimPhase::Advance:
    return "Advance";
  case Render::GL::CombatAnimPhase::WindUp:
    return "WindUp";
  case Render::GL::CombatAnimPhase::Strike:
    return "Strike";
  case Render::GL::CombatAnimPhase::Impact:
    return "Impact";
  case Render::GL::CombatAnimPhase::Recover:
    return "Recover";
  case Render::GL::CombatAnimPhase::Reposition:
    return "Reposition";
  }
  return "Unknown";
}

auto locomotion_state_name(Render::Creature::MovementAnimationState state) noexcept
    -> const char* {
  switch (state) {
  case Render::Creature::MovementAnimationState::Idle:
    return "Idle";
  case Render::Creature::MovementAnimationState::Walk:
    return "Walk";
  case Render::Creature::MovementAnimationState::Run:
    return "Run";
  }
  return "Unknown";
}

auto animation_state_name(Render::Creature::AnimationStateId state) noexcept -> const
    char* {
  using A = Render::Creature::AnimationStateId;
  switch (state) {
  case A::Idle:
    return "Idle";
  case A::Walk:
    return "Walk";
  case A::Run:
    return "Run";
  case A::Hold:
    return "Hold";
  case A::AttackMelee:
    return "AttackMelee";
  case A::AttackSword:
    return "AttackSword";
  case A::AttackSpear:
    return "AttackSpear";
  case A::AttackRanged:
    return "AttackRanged";
  case A::AttackBow:
    return "AttackBow";
  case A::Cast:
    return "Cast";
  case A::Die:
    return "Die";
  case A::Dead:
    return "Dead";
  case A::RidingIdle:
    return "RidingIdle";
  case A::RidingCharge:
    return "RidingCharge";
  case A::RidingReining:
    return "RidingReining";
  case A::RidingBowShot:
    return "RidingBowShot";
  case A::RpgSwordSlashLeft:
    return "RpgSwordSlashLeft";
  case A::RpgSwordSlashRight:
    return "RpgSwordSlashRight";
  case A::RpgSwordOverhead:
    return "RpgSwordOverhead";
  case A::RpgSwordThrust:
    return "RpgSwordThrust";
  case A::RpgSwordFinisher:
    return "RpgSwordFinisher";
  case A::Count:
    return "Count";
  }
  return "Unknown";
}

auto soldier_cull_reason_name(SoldierCullReason reason) noexcept -> const char* {
  switch (reason) {
  case SoldierCullReason::None:
    return "None";
  case SoldierCullReason::Frustum:
    return "Frustum";
  case SoldierCullReason::Billboard:
    return "Billboard";
  case SoldierCullReason::Temporal:
    return "Temporal";
  }
  return "Unknown";
}

auto soldier_visual_state_name(SoldierVisualState state) noexcept -> const char* {
  switch (state) {
  case SoldierVisualState::Idle:
    return "Idle";
  case SoldierVisualState::Walk:
    return "Walk";
  case SoldierVisualState::Run:
    return "Run";
  case SoldierVisualState::Attack:
    return "Attack";
  case SoldierVisualState::Hold:
    return "Hold";
  case SoldierVisualState::HitReaction:
    return "HitReaction";
  case SoldierVisualState::Dying:
    return "Dying";
  case SoldierVisualState::Dead:
    return "Dead";
  }
  return "Unknown";
}

auto CombatAnimationDiagnostics::instance() -> CombatAnimationDiagnostics& {
  static CombatAnimationDiagnostics diagnostics;
  return diagnostics;
}

void CombatAnimationDiagnostics::set_enabled(bool enabled) {
  m_enabled = enabled;
  if (!active()) {
    m_units.clear();
  }
}

void CombatAnimationDiagnostics::set_logging_enabled(bool enabled) {
  m_logging_enabled = enabled;
}

void CombatAnimationDiagnostics::begin_frame(std::uint64_t frame_index) {
  m_frame_index = frame_index;
  if (!active()) {
    return;
  }
  m_units.clear();
  m_mode_indicator_entities.clear();
}

void CombatAnimationDiagnostics::record_unit_sample(
    const UnitAnimationDebugSample& sample) {
  if (!active()) {
    return;
  }
  auto& entry = m_units[sample.entity_id];
  entry.unit = sample;
}

void CombatAnimationDiagnostics::mark_elephant_override(std::uint32_t entity_id) {
  if (!active()) {
    return;
  }
  m_units[entity_id].unit.elephant_attack_override = true;
}

void CombatAnimationDiagnostics::record_soldier_sample(
    std::uint32_t entity_id, const SoldierAnimationDebugSample& sample) {
  if (!active()) {
    return;
  }

  SoldierAnimationDebugSample recorded = sample;
  recorded.visual_state = visual_state_from_sample(recorded);

  SoldierKey const key{
      entity_id,
      static_cast<std::uint16_t>(std::clamp(recorded.soldier_index, 0, 0xFFFF))};
  auto& tracker = m_trackers[key];

  if (tracker.has_previous && recorded.sample_time < tracker.last_time) {
    tracker = SoldierTracker{};
  }

  if (tracker.has_previous) {
    recorded.visual_state_changed = recorded.visual_state != tracker.last_visual_state;
    recorded.movement_state_changed =
        recorded.locomotion_state != tracker.last_locomotion;
    recorded.variant_changed = recorded.attack_variant != tracker.last_attack_variant;
    recorded.lod_changed = recorded.lod != tracker.last_lod;
    recorded.attack_phase_reset =
        recorded.is_attacking &&
        (tracker.last_visual_state == SoldierVisualState::Attack) &&
        (recorded.attack_phase + k_attack_phase_reset_epsilon <
         tracker.last_attack_phase);
  }

  if (recorded.visual_state_changed) {

    bool const hit_transition =
        recorded.visual_state == SoldierVisualState::HitReaction ||
        tracker.last_visual_state == SoldierVisualState::HitReaction;
    if (!hit_transition) {
      tracker.transition_times.push_back(recorded.sample_time);
      tracker.transition_states.push_back(recorded.visual_state);
    }
  }
  if (recorded.visual_state == SoldierVisualState::Dying ||
      recorded.visual_state == SoldierVisualState::Dead) {

    tracker.transition_times.clear();
    tracker.transition_states.clear();
  }
  while (!tracker.transition_times.empty() &&
         recorded.sample_time - tracker.transition_times.front() >
             k_churn_window_seconds) {
    tracker.transition_times.erase(tracker.transition_times.begin());
    tracker.transition_states.erase(tracker.transition_states.begin());
  }

  recorded.transitions_last_second =
      static_cast<std::uint32_t>(tracker.transition_times.size());
  std::uint32_t rapid_cycle_returns = 0U;
  for (std::size_t index = 2; index < tracker.transition_times.size(); ++index) {
    for (std::size_t const cycle_length : {2U, 3U}) {
      if (index >= cycle_length &&
          tracker.transition_states[index] ==
              tracker.transition_states[index - cycle_length] &&
          tracker.transition_times[index] -
                  tracker.transition_times[index - cycle_length] <
              k_rapid_cycle_seconds) {
        ++rapid_cycle_returns;
        break;
      }
    }
  }

  recorded.churn_flagged =
      (recorded.transitions_last_second >= k_churn_threshold &&
       rapid_cycle_returns >= 2U) ||
      recorded.transitions_last_second >= k_extreme_churn_threshold;

  if (recorded.churn_flagged && m_logging_enabled &&
      tracker.last_logged_frame != m_frame_index) {
    tracker.last_logged_frame = m_frame_index;
    qWarning().noquote() << QStringLiteral("SOI combat animation churn: entity=%1 "
                                           "soldier=%2 transitions=%3 state=%4")
                                .arg(entity_id)
                                .arg(recorded.soldier_index)
                                .arg(recorded.transitions_last_second)
                                .arg(QString::fromLatin1(
                                    soldier_visual_state_name(recorded.visual_state)));
  }

  tracker.has_previous = true;
  tracker.last_time = recorded.sample_time;
  tracker.last_attack_phase = recorded.attack_phase;
  tracker.last_attack_variant = recorded.attack_variant;
  tracker.last_locomotion = recorded.locomotion_state;
  tracker.last_lod = recorded.lod;
  tracker.last_visual_state = recorded.visual_state;

  auto& unit = m_units[entity_id];
  unit.unit.entity_id = entity_id;
  unit.soldiers.push_back(recorded);
  if (recorded.churn_flagged) {
    ++unit.flagged_soldiers;
  }
}

void CombatAnimationDiagnostics::record_submitted_body_pose(std::uint32_t entity_id,
                                                            std::uint16_t soldier_index,
                                                            float body_up_y,
                                                            float max_arm_reach) {
  if (!active()) {
    return;
  }
  auto found = m_units.find(entity_id);
  if (found == m_units.end()) {
    return;
  }
  auto& soldiers = found->second.soldiers;
  auto const sample = std::find_if(
      soldiers.rbegin(), soldiers.rend(), [soldier_index](const auto& soldier) {
        return soldier.soldier_index == static_cast<int>(soldier_index);
      });
  if (sample == soldiers.rend()) {
    return;
  }
  sample->submitted_body_up_y = std::clamp(body_up_y, -1.0F, 1.0F);
  sample->submitted_body_tilt_degrees =
      qRadiansToDegrees(std::acos(sample->submitted_body_up_y));
  sample->submitted_max_arm_reach = std::max(0.0F, max_arm_reach);
  sample->submitted_body_pose_valid = true;
}

void CombatAnimationDiagnostics::record_mode_indicator(std::uint32_t entity_id) {
  if (active() && entity_id != 0U) {
    m_mode_indicator_entities.insert(entity_id);
  }
}

auto CombatAnimationDiagnostics::mode_indicator_submitted(
    std::uint32_t entity_id) const noexcept -> bool {
  return active() && m_mode_indicator_entities.contains(entity_id);
}

auto CombatAnimationDiagnostics::find_unit(std::uint32_t entity_id) const
    -> const CombatAnimationDebugUnit* {
  if (!active()) {
    return nullptr;
  }
  auto const found = m_units.find(entity_id);
  return found == m_units.end() ? nullptr : &found->second;
}

auto CombatAnimationDiagnostics::flagged_unit_count() const noexcept -> std::size_t {
  return std::count_if(m_units.begin(), m_units.end(), [](const auto& entry) {
    return entry.second.flagged_soldiers > 0U;
  });
}

} // namespace Render::Profiling
