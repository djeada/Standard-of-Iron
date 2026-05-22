#include <gtest/gtest.h>

#include "render/creature/combat_visual_state.h"

namespace {

using Render::Creature::CombatVisualInterruptReason;
using Render::Creature::CombatVisualTransactionPhase;
using Render::Creature::SoldierCombatLane;

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
  EXPECT_TRUE(retarget.persistent.queued_next.pending);
  EXPECT_EQ(retarget.persistent.queued_next.target_id, 99U);

  raw.sample_time += 0.50F;
  raw.attack_requested = true;
  raw.attack_target_alive = true;
  raw.attack_target_id = 99U;
  auto completed = Render::Creature::resolve_combat_visual_state(
      retarget.persistent, raw, lane.profile);
  raw.sample_time += 0.25F;
  completed = Render::Creature::resolve_combat_visual_state(
      completed.persistent, raw, lane.profile);
  ASSERT_TRUE(completed.resolved.active);
  ASSERT_EQ(completed.resolved.phase, CombatVisualTransactionPhase::ExitBlend);
  raw.sample_time += 0.20F;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.20F;
  auto next = Render::Creature::resolve_combat_visual_state(
      completed.persistent, raw, lane.profile);
  EXPECT_TRUE(next.resolved.active);
  EXPECT_GT(next.resolved.transaction_id, first.resolved.transaction_id);
  EXPECT_EQ(next.resolved.source_target_id, 99U);
}

TEST(CombatVisualState, RepeatedAttackCycleDoesNotHardResetFromRecoverToEnter) {
  Render::Creature::CombatLaneInputs lane_inputs{};
  lane_inputs.unit_seed = 17U;
  lane_inputs.soldier_seed = 41U;
  lane_inputs.is_melee = true;
  lane_inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  auto const lane = Render::Creature::resolve_soldier_combat_lane({}, lane_inputs);

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 1.00F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 11U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Reposition;
  raw.combat_phase_progress = 0.95F;

  auto late = Render::Creature::resolve_combat_visual_state({}, raw, lane.profile);
  ASSERT_TRUE(late.resolved.active);
  ASSERT_GT(late.resolved.attack_phase, 0.90F);

  raw.sample_time += 0.02F;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.0F;
  auto bridged =
      Render::Creature::resolve_combat_visual_state(late.persistent, raw, lane.profile);

  EXPECT_TRUE(bridged.resolved.active);
  EXPECT_TRUE(bridged.persistent.queued_next.pending);
  EXPECT_EQ(bridged.resolved.transaction_id, late.resolved.transaction_id);
  EXPECT_EQ(bridged.resolved.phase, CombatVisualTransactionPhase::ExitBlend);
  EXPECT_GE(bridged.resolved.attack_phase, late.resolved.attack_phase);

  raw.sample_time += 0.04F;
  auto still_bridging = Render::Creature::resolve_combat_visual_state(
      bridged.persistent, raw, lane.profile);
  EXPECT_TRUE(still_bridging.resolved.active);
  EXPECT_TRUE(still_bridging.persistent.queued_next.pending);
  EXPECT_EQ(still_bridging.resolved.transaction_id, late.resolved.transaction_id);
  EXPECT_EQ(still_bridging.resolved.phase, CombatVisualTransactionPhase::ExitBlend);

  Render::Creature::CombatVisualTimingConfig fast_bridge{};
  fast_bridge.minimum_hold_duration = 0.01F;
  fast_bridge.recover_duration = 0.05F;
  fast_bridge.exit_blend_duration = 0.05F;
  fast_bridge.new_transaction_reset_threshold = 0.18F;
  raw.sample_time += 0.08F;
  auto queued_restart = Render::Creature::resolve_combat_visual_state(
      still_bridging.persistent, raw, lane.profile, fast_bridge);
  EXPECT_TRUE(queued_restart.persistent.active);
  EXPECT_FALSE(queued_restart.persistent.queued_next.pending);
  EXPECT_GT(queued_restart.persistent.transaction_id, late.resolved.transaction_id);
  EXPECT_LT(queued_restart.persistent.attack_phase, 0.20F);
}

TEST(CombatVisualState, ActiveTransactionKeepsLockedLaneWhenPressureDrops) {
  Render::Creature::CombatLaneProfile high_pressure_lane{};
  high_pressure_lane.lane = SoldierCombatLane::StepIn;
  high_pressure_lane.phase_bias = -0.015F;
  high_pressure_lane.recover_scale = 1.00F;
  high_pressure_lane.emphasis_scale = 1.00F;

  Render::Creature::CombatLaneProfile low_pressure_lane{};
  low_pressure_lane.lane = SoldierCombatLane::ShieldBrace;
  low_pressure_lane.phase_bias = -0.060F;
  low_pressure_lane.recover_scale = 1.08F;
  low_pressure_lane.emphasis_scale = 0.94F;

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 1.0F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 11U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Strike;
  raw.combat_phase_progress = 0.35F;

  auto started =
      Render::Creature::resolve_combat_visual_state({}, raw, high_pressure_lane);
  ASSERT_TRUE(started.resolved.active);
  ASSERT_EQ(started.resolved.lane, SoldierCombatLane::StepIn);
  ASSERT_EQ(started.persistent.locked_lane, SoldierCombatLane::StepIn);

  raw.sample_time += 0.02F;
  raw.combat_phase = Engine::Core::CombatAnimationState::Impact;
  raw.combat_phase_progress = 0.25F;
  auto continued = Render::Creature::resolve_combat_visual_state(
      started.persistent, raw, low_pressure_lane);

  EXPECT_TRUE(continued.resolved.active);
  EXPECT_EQ(continued.resolved.lane, SoldierCombatLane::StepIn);
  EXPECT_EQ(continued.persistent.locked_lane, SoldierCombatLane::StepIn);
}

TEST(CombatVisualState,
     QueuedFollowupReusesAttackLaneWhenIncomingLaneWouldBraceShield) {
  Render::Creature::CombatLaneProfile attack_lane{};
  attack_lane.lane = SoldierCombatLane::StepIn;
  attack_lane.phase_bias = -0.015F;
  attack_lane.recover_scale = 1.00F;
  attack_lane.emphasis_scale = 1.00F;

  Render::Creature::CombatLaneProfile brace_lane{};
  brace_lane.lane = SoldierCombatLane::ShieldBrace;
  brace_lane.phase_bias = -0.060F;
  brace_lane.recover_scale = 1.08F;
  brace_lane.emphasis_scale = 0.94F;

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 1.0F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 11U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Reposition;
  raw.combat_phase_progress = 0.95F;

  auto late = Render::Creature::resolve_combat_visual_state({}, raw, attack_lane);
  ASSERT_TRUE(late.resolved.active);
  ASSERT_EQ(late.persistent.locked_lane, SoldierCombatLane::StepIn);

  raw.sample_time += 0.02F;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.0F;
  auto queued =
      Render::Creature::resolve_combat_visual_state(late.persistent, raw, brace_lane);
  ASSERT_TRUE(queued.persistent.queued_next.pending);
  EXPECT_EQ(queued.persistent.queued_next.lane, SoldierCombatLane::StepIn);

  Render::Creature::CombatVisualTimingConfig fast_queue{};
  fast_queue.minimum_hold_duration = 0.01F;
  fast_queue.recover_duration = 0.05F;
  fast_queue.exit_blend_duration = 0.05F;
  fast_queue.new_transaction_reset_threshold = 0.18F;
  raw.sample_time += 0.16F;
  auto restarted = Render::Creature::resolve_combat_visual_state(
      queued.persistent, raw, brace_lane, fast_queue);
  EXPECT_TRUE(restarted.persistent.active);
  EXPECT_EQ(restarted.persistent.locked_lane, SoldierCombatLane::StepIn);
}

TEST(CombatVisualState,
     RestartAfterCompletionReusesPreviousAttackLaneWhenIncomingLaneWouldBraceShield) {
  Render::Creature::CombatLaneProfile attack_lane{};
  attack_lane.lane = SoldierCombatLane::StepIn;
  attack_lane.phase_bias = -0.015F;
  attack_lane.recover_scale = 1.00F;
  attack_lane.emphasis_scale = 1.00F;

  Render::Creature::CombatLaneProfile brace_lane{};
  brace_lane.lane = SoldierCombatLane::ShieldBrace;
  brace_lane.phase_bias = -0.060F;
  brace_lane.recover_scale = 1.08F;
  brace_lane.emphasis_scale = 0.94F;

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 2.0F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 11U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Strike;
  raw.combat_phase_progress = 0.30F;

  auto started = Render::Creature::resolve_combat_visual_state({}, raw, attack_lane);
  ASSERT_TRUE(started.resolved.active);

  Render::Creature::CombatVisualTimingConfig fast_finish{};
  fast_finish.minimum_hold_duration = 0.01F;
  fast_finish.recover_duration = 0.05F;
  fast_finish.exit_blend_duration = 0.05F;
  fast_finish.new_transaction_reset_threshold = 0.18F;
  raw.sample_time += 0.08F;
  raw.attack_requested = false;
  raw.combat_phase = Engine::Core::CombatAnimationState::Idle;
  raw.combat_phase_progress = 0.0F;
  auto recovering = Render::Creature::resolve_combat_visual_state(
      started.persistent, raw, attack_lane, fast_finish);
  raw.sample_time += 0.60F;
  auto settled = Render::Creature::resolve_combat_visual_state(
      recovering.persistent, raw, attack_lane, fast_finish);
  raw.sample_time += 0.02F;
  settled = Render::Creature::resolve_combat_visual_state(
      settled.persistent, raw, attack_lane, fast_finish);
  ASSERT_FALSE(settled.persistent.active);
  ASSERT_EQ(settled.persistent.locked_lane, SoldierCombatLane::StepIn);

  raw.sample_time += 0.02F;
  raw.attack_requested = true;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.05F;
  auto restarted = Render::Creature::resolve_combat_visual_state(
      settled.persistent, raw, brace_lane);
  EXPECT_TRUE(restarted.resolved.active);
  EXPECT_EQ(restarted.persistent.locked_lane, SoldierCombatLane::StepIn);
  EXPECT_EQ(restarted.resolved.lane, SoldierCombatLane::StepIn);
}

TEST(CombatVisualState,
     InitialAttackOnDefensiveRearLanePromotesToSupportStrikeInsteadOfShieldDrop) {
  Render::Creature::CombatLaneProfile rear_lane{};
  rear_lane.lane = SoldierCombatLane::IdleReady;
  rear_lane.phase_bias = -0.145F;
  rear_lane.recover_scale = 1.15F;
  rear_lane.emphasis_scale = 0.88F;

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 3.0F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 42U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.15F;

  auto started = Render::Creature::resolve_combat_visual_state({}, raw, rear_lane);
  EXPECT_TRUE(started.resolved.active);
  EXPECT_EQ(started.persistent.locked_lane, SoldierCombatLane::SupportStrike);
  EXPECT_EQ(started.resolved.lane, SoldierCombatLane::SupportStrike);
}

TEST(CombatVisualState, QueuedAttackClearsWhenTargetDiesBeforeDequeue) {
  Render::Creature::CombatLaneProfile lane{};
  lane.lane = SoldierCombatLane::LeadStrike;
  lane.phase_bias = 0.075F;
  lane.recover_scale = 0.92F;
  lane.emphasis_scale = 1.10F;

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 1.0F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 11U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Reposition;
  raw.combat_phase_progress = 0.95F;

  auto late = Render::Creature::resolve_combat_visual_state({}, raw, lane);
  ASSERT_TRUE(late.resolved.active);

  raw.sample_time += 0.02F;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.0F;
  auto queued =
      Render::Creature::resolve_combat_visual_state(late.persistent, raw, lane);
  ASSERT_TRUE(queued.persistent.queued_next.pending);

  raw.sample_time += 0.08F;
  raw.attack_target_alive = false;
  auto expiring =
      Render::Creature::resolve_combat_visual_state(queued.persistent, raw, lane);
  ASSERT_TRUE(expiring.resolved.active);

  raw.sample_time += 0.20F;
  auto settled =
      Render::Creature::resolve_combat_visual_state(expiring.persistent, raw, lane);
  EXPECT_FALSE(settled.persistent.active);
  EXPECT_FALSE(settled.persistent.queued_next.pending);

  raw.sample_time += 0.02F;
  settled =
      Render::Creature::resolve_combat_visual_state(settled.persistent, raw, lane);
  EXPECT_FALSE(settled.resolved.active);
}

TEST(CombatVisualState, FamilyChangeQueuesFollowupUntilCurrentCompletes) {
  Render::Creature::CombatLaneProfile sword_lane{};
  sword_lane.lane = SoldierCombatLane::LeadStrike;
  sword_lane.phase_bias = 0.075F;
  sword_lane.recover_scale = 0.92F;
  sword_lane.emphasis_scale = 1.10F;

  Render::Creature::CombatLaneProfile spear_lane{};
  spear_lane.lane = SoldierCombatLane::StepIn;
  spear_lane.phase_bias = -0.015F;
  spear_lane.recover_scale = 1.00F;
  spear_lane.emphasis_scale = 1.00F;

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 2.0F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.attack_target_id = 55U;
  raw.attack_target_alive = true;
  raw.combat_phase = Engine::Core::CombatAnimationState::Strike;
  raw.combat_phase_progress = 0.40F;

  auto started = Render::Creature::resolve_combat_visual_state({}, raw, sword_lane);
  ASSERT_TRUE(started.resolved.active);
  ASSERT_EQ(started.resolved.attack_family, Engine::Core::CombatAttackFamily::Sword);

  raw.sample_time += 0.03F;
  raw.attack_family = Engine::Core::CombatAttackFamily::Spear;
  raw.combat_phase = Engine::Core::CombatAnimationState::Advance;
  raw.combat_phase_progress = 0.10F;
  auto queued = Render::Creature::resolve_combat_visual_state(
      started.persistent, raw, spear_lane);

  EXPECT_TRUE(queued.resolved.active);
  EXPECT_EQ(queued.resolved.attack_family, Engine::Core::CombatAttackFamily::Sword);
  EXPECT_TRUE(queued.persistent.queued_next.pending);
  EXPECT_EQ(queued.persistent.queued_next.family,
            Engine::Core::CombatAttackFamily::Spear);

  raw.sample_time += 0.40F;
  auto exiting =
      Render::Creature::resolve_combat_visual_state(queued.persistent, raw, spear_lane);
  ASSERT_TRUE(exiting.resolved.active);
  ASSERT_EQ(exiting.resolved.phase, CombatVisualTransactionPhase::ExitBlend);

  raw.sample_time += 0.20F;
  auto next = Render::Creature::resolve_combat_visual_state(
      exiting.persistent, raw, spear_lane);
  EXPECT_TRUE(next.persistent.active);
  EXPECT_EQ(next.persistent.locked_family, Engine::Core::CombatAttackFamily::Spear);
  EXPECT_GT(next.persistent.transaction_id, started.resolved.transaction_id);
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

TEST(CombatVisualState, CombatPhaseProgressIsEasedIntoVisualAttackPhase) {
  Render::Creature::CombatLaneProfile const lane{};

  Render::Creature::CombatVisualRawInputs raw{};
  raw.sample_time = 0.10F;
  raw.attack_requested = true;
  raw.is_melee = true;
  raw.attack_family = Engine::Core::CombatAttackFamily::Sword;
  raw.combat_phase = Engine::Core::CombatAnimationState::WindUp;
  raw.combat_phase_progress = 0.50F;

  auto windup = Render::Creature::resolve_combat_visual_state({}, raw, lane);
  EXPECT_LT(windup.resolved.attack_phase, 0.24F);
  EXPECT_GT(windup.resolved.attack_phase, 0.21F);

  raw.sample_time = 0.20F;
  raw.combat_phase = Engine::Core::CombatAnimationState::Strike;
  raw.combat_phase_progress = 0.50F;
  auto strike = Render::Creature::resolve_combat_visual_state({}, raw, lane);
  EXPECT_GT(strike.resolved.attack_phase, 0.46F);
  EXPECT_LT(strike.resolved.attack_phase, 0.49F);
}

} // namespace
