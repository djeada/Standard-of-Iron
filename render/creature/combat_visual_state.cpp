#include "combat_visual_state.h"

#include "animation_core_bridge.h"

namespace Render::Creature {

namespace {

auto animation_lane_inputs(const CombatLaneInputs& inputs) noexcept
    -> Animation::CombatLaneInputs {
  return Animation::CombatLaneInputs{
      .unit_seed = inputs.unit_seed,
      .soldier_seed = inputs.soldier_seed,
      .row = inputs.row,
      .col = inputs.col,
      .rows = inputs.rows,
      .cols = inputs.cols,
      .health_ratio = inputs.health_ratio,
      .local_enemy_pressure = inputs.local_enemy_pressure,
      .force_single_soldier = inputs.force_single_soldier,
      .is_melee = inputs.is_melee,
      .is_mounted = inputs.is_mounted,
      .attack_family = animation_attack_family(inputs.attack_family),
  };
}

auto animation_raw_inputs(const CombatVisualRawInputs& raw) noexcept
    -> Animation::CombatRawInputs {
  return Animation::CombatRawInputs{
      .sample_time = raw.sample_time,
      .attack_offset = raw.attack_offset,
      .has_attack_offset = raw.has_attack_offset,
      .attack_requested = raw.attack_requested,
      .is_melee = raw.is_melee,
      .is_mounted = raw.is_mounted,
      .is_casting = raw.is_casting,
      .finisher_attack = raw.finisher_attack,
      .amplified_attack = raw.amplified_attack,
      .is_hit_reacting = raw.is_hit_reacting,
      .is_healing = raw.is_healing,
      .is_constructing = raw.is_constructing,
      .is_dying = raw.is_dying,
      .is_dead = raw.is_dead,
      .locomotion = raw.locomotion,
      .combat_phase = animation_combat_phase(raw.combat_phase),
      .combat_phase_progress = raw.combat_phase_progress,
      .attack_variant = raw.attack_variant,
      .attack_target_id = raw.attack_target_id,
      .attack_target_alive = raw.attack_target_alive,
      .attack_family = animation_attack_family(raw.attack_family),
  };
}

} // namespace

auto default_combat_visual_timing_config() noexcept -> const CombatVisualTimingConfig& {
  return Animation::default_combat_timing_config();
}

auto resolve_soldier_combat_lane(const SoldierCombatLaneState& previous,
                                 const CombatLaneInputs& inputs) noexcept
    -> SoldierCombatLaneResolution {
  auto const animation_resolution =
      Animation::resolve_soldier_combat_lane(previous, animation_lane_inputs(inputs));
  return SoldierCombatLaneResolution{animation_resolution.state,
                                     animation_resolution.profile};
}

auto next_attack_variant_for_lane(std::uint8_t base_variant,
                                  Engine::Core::CombatAttackFamily family,
                                  SoldierCombatLane lane,
                                  float local_enemy_pressure,
                                  std::uint8_t variant_bias,
                                  std::uint32_t lane_seed,
                                  std::uint32_t transaction_id) noexcept
    -> std::uint8_t {
  return Animation::next_attack_variant_for_lane(base_variant,
                                                 animation_attack_family(family),
                                                 lane,
                                                 local_enemy_pressure,
                                                 variant_bias,
                                                 lane_seed,
                                                 transaction_id);
}

auto resolve_combat_visual_state(const CombatVisualPersistentState& previous,
                                 const CombatVisualRawInputs& raw,
                                 const CombatLaneProfile& lane,
                                 const CombatVisualTimingConfig& config) noexcept
    -> CombatVisualStateResolution {
  return Animation::resolve_combat_transaction_state(
      previous, animation_raw_inputs(raw), lane, config);
}

} // namespace Render::Creature
