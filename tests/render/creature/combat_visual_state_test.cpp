#include <gtest/gtest.h>

#include "render/creature/combat_visual_state.h"

namespace {

using Render::Creature::CombatVisualInterruptReason;
using Render::Creature::CombatVisualTransactionPhase;

TEST(CombatVisualState, MonotonicRecoveryAndExitBlendPersistAfterAttackStops) {
  Render::Creature::CombatLaneInputs lane_inputs{};
  lane_inputs.unit_seed = 17U;
  lane_inputs.soldier_seed = 41U;
  lane_inputs.is_melee = true;
  lane_inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  auto const lane = Render::Creature::resolve_soldier_combat_lane({}, lane_inputs);

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 0.20F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.combat_phase = Engine::Core::CombatAnimationState::Strike;
  raw.combat_phase_progress = 0.50F;

  auto active = Render::Creature::resolve_combat_visual_state({}, raw, lane.profile);
  ASSERT_TRUE(active.resolved.active);
  float const active_phase = active.resolved.attack_phase;

  raw.sample_time += 0.05F;
  raw.attack_requested = false;
  raw.combat_phase = Engine::Core::CombatAnimationState::Idle;
  raw.combat_phase_progress = 0.0F;
  auto recovering = Render::Creature::resolve_combat_visual_state(
      active.persistent, raw, lane.profile);
  EXPECT_TRUE(recovering.resolved.active);
  EXPECT_GT(recovering.resolved.attack_phase, active_phase);
  EXPECT_EQ(recovering.resolved.phase, CombatVisualTransactionPhase::Recover);

  raw.sample_time += 0.30F;
  auto exit_blending = Render::Creature::resolve_combat_visual_state(
      recovering.persistent, raw, lane.profile);
  EXPECT_TRUE(exit_blending.resolved.active);
  EXPECT_EQ(exit_blending.resolved.phase, CombatVisualTransactionPhase::ExitBlend);
  EXPECT_LT(exit_blending.resolved.exit_blend_progress, 1.0F);

  raw.sample_time += 0.20F;
  auto settled = Render::Creature::resolve_combat_visual_state(
      exit_blending.persistent, raw, lane.profile);
  EXPECT_TRUE(settled.resolved.active);
  EXPECT_EQ(settled.resolved.phase, CombatVisualTransactionPhase::ExitBlend);

  raw.sample_time += 0.02F;
  settled = Render::Creature::resolve_combat_visual_state(
      settled.persistent, raw, lane.profile);
  EXPECT_FALSE(settled.resolved.active);
  EXPECT_EQ(settled.persistent.interruption_reason,
            CombatVisualInterruptReason::NormalComplete);
}

TEST(CombatVisualState, RetargetKeepsLockedVariantUntilTransactionEnds) {
  Render::Creature::CombatLaneInputs lane_inputs{};
  lane_inputs.unit_seed = 71U;
  lane_inputs.soldier_seed = 19U;
  lane_inputs.is_melee = true;
  lane_inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  auto const lane = Render::Creature::resolve_soldier_combat_lane({}, lane_inputs);

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 0.15F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_variant = 1U;
  raw.attack_target_id = 11U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Strike;
  raw.combat_phase_progress = 0.35F;

  auto first = Render::Creature::resolve_combat_visual_state({}, raw, lane.profile);
  ASSERT_TRUE(first.resolved.active);

  raw.sample_time += 0.02F;
  raw.attack_variant = 2U;
  raw.attack_target_id = 99U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.10F;
  auto retarget = Render::Creature::resolve_combat_visual_state(
      first.persistent, raw, lane.profile);

  EXPECT_TRUE(retarget.resolved.active);
  EXPECT_EQ(retarget.resolved.transaction_id, first.resolved.transaction_id);
  EXPECT_EQ(retarget.resolved.attack_variant, first.resolved.attack_variant);
  EXPECT_EQ(retarget.persistent.source_target_id, first.persistent.source_target_id);

  raw.sample_time += 0.50F;
  raw.attack_requested = false;
  raw.attack_target_alive = false;
  raw.attack_target_id = 0U;
  auto completed = Render::Creature::resolve_combat_visual_state(
      retarget.persistent, raw, lane.profile);
  raw.sample_time += 0.25F;
  completed = Render::Creature::resolve_combat_visual_state(
      completed.persistent, raw, lane.profile);
  EXPECT_TRUE(completed.resolved.active);
  EXPECT_EQ(completed.resolved.phase, CombatVisualTransactionPhase::ExitBlend);
  raw.sample_time += 0.02F;
  completed = Render::Creature::resolve_combat_visual_state(
      completed.persistent, raw, lane.profile);
  EXPECT_FALSE(completed.resolved.active);

  raw.sample_time += 0.02F;
  raw.attack_requested = true;
  raw.attack_variant = 2U;
  raw.attack_target_id = 99U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.20F;
  auto next = Render::Creature::resolve_combat_visual_state(
      completed.persistent, raw, lane.profile);
  EXPECT_TRUE(next.resolved.active);
  EXPECT_GT(next.resolved.transaction_id, first.resolved.transaction_id);
  EXPECT_EQ(next.resolved.source_target_id, 99U);
}

TEST(CombatVisualState, SoldierLaneIsStableForMatchingInputs) {
  Render::Creature::CombatLaneInputs inputs{};
  inputs.unit_seed = 5U;
  inputs.soldier_seed = 9U;
  inputs.row = 1;
  inputs.col = 2;
  inputs.rows = 3;
  inputs.cols = 5;
  inputs.health_ratio = 0.9F;
  inputs.is_melee = true;
  inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;

  auto const first = Render::Creature::resolve_soldier_combat_lane({}, inputs);
  auto const second =
      Render::Creature::resolve_soldier_combat_lane(first.state, inputs);

  EXPECT_TRUE(first.state.initialized);
  EXPECT_EQ(second.state.signature, first.state.signature);
  EXPECT_EQ(second.state.lane_seed, first.state.lane_seed);
  EXPECT_EQ(second.state.lane, first.state.lane);
  EXPECT_FLOAT_EQ(second.profile.phase_bias, first.profile.phase_bias);
}

TEST(CombatVisualState, LocalEnemyPressureParticipatesInLaneSignature) {
  Render::Creature::CombatLaneInputs inputs{};
  inputs.unit_seed = 5U;
  inputs.soldier_seed = 9U;
  inputs.row = 0;
  inputs.col = 1;
  inputs.rows = 3;
  inputs.cols = 5;
  inputs.health_ratio = 1.0F;
  inputs.is_melee = true;
  inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  inputs.local_enemy_pressure = 0.0F;

  auto const low_pressure = Render::Creature::resolve_soldier_combat_lane({}, inputs);

  inputs.local_enemy_pressure = 1.0F;
  auto const high_pressure =
      Render::Creature::resolve_soldier_combat_lane(low_pressure.state, inputs);

  EXPECT_NE(high_pressure.state.signature, low_pressure.state.signature);
}

TEST(CombatVisualState, VariantSelectionUsesFamilyLaneAndPressure) {
  auto const sword_support = Render::Creature::next_attack_variant_for_lane(
      1U,
      Engine::Core::CombatAttackFamily::Sword,
      Render::Creature::SoldierCombatLane::SupportStrike,
      0.0F,
      0U,
      19U,
      3U);
  auto const spear_brace = Render::Creature::next_attack_variant_for_lane(
      1U,
      Engine::Core::CombatAttackFamily::Spear,
      Render::Creature::SoldierCombatLane::ShieldBrace,
      1.0F,
      0U,
      19U,
      3U);

  EXPECT_NE(spear_brace, sword_support);
}

} // namespace
