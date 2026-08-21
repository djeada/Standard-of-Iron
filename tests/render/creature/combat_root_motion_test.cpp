#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "animation/combat_root_motion_manifest.h"
#include "animation/reaction_pose_manifest.h"

namespace {

using Animation::CombatAttackFamily;
using Animation::CombatRootMotionInputs;
using Animation::CombatTransactionPhase;
using Animation::HitReactionForm;
using Animation::melee_lunge_offset;
using Animation::MeleeSwingOutcome;
using Animation::resolve_combat_root_motion;

TEST(CombatRootMotion, ASwordCutCarriesTheBodyForwardAndBringsItBack) {
  float peak = 0.0F;
  float peak_phase = 0.0F;
  for (int i = 0; i <= 100; ++i) {
    float const phase = static_cast<float>(i) / 100.0F;
    float const offset = melee_lunge_offset(
        CombatAttackFamily::Sword, phase, MeleeSwingOutcome::Clean, false);
    if (offset > peak) {
      peak = offset;
      peak_phase = phase;
    }
  }
  EXPECT_GT(peak, 0.18F);
  EXPECT_LT(peak, 0.35F);
  EXPECT_GT(peak_phase, 0.45F);
  EXPECT_LT(peak_phase, 0.70F);
  EXPECT_NEAR(melee_lunge_offset(
                  CombatAttackFamily::Sword, 0.0F, MeleeSwingOutcome::Clean, false),
              0.0F,
              1.0e-4F);
  EXPECT_NEAR(melee_lunge_offset(
                  CombatAttackFamily::Sword, 1.0F, MeleeSwingOutcome::Clean, false),
              0.0F,
              1.0e-4F);
  EXPECT_LT(melee_lunge_offset(
                CombatAttackFamily::Sword, 0.12F, MeleeSwingOutcome::Clean, false),
            0.0F)
      << "the weight loads onto the rear foot before the drive";
}

TEST(CombatRootMotion, AWhiffOverextendsAndFormationRanksStepShorter) {
  float const clean = melee_lunge_offset(
      CombatAttackFamily::Spear, 0.6F, MeleeSwingOutcome::Clean, false);
  float const whiff = melee_lunge_offset(
      CombatAttackFamily::Spear, 0.6F, MeleeSwingOutcome::Evaded, false);
  float const ranked = melee_lunge_offset(
      CombatAttackFamily::Spear, 0.6F, MeleeSwingOutcome::Clean, true);
  EXPECT_GT(whiff, clean);
  EXPECT_LT(ranked, clean);
}

TEST(CombatRootMotion, LungeIsContinuousAcrossThePhase) {
  float previous = melee_lunge_offset(
      CombatAttackFamily::Sword, 0.0F, MeleeSwingOutcome::Heavy, false);
  for (int i = 1; i <= 400; ++i) {
    float const phase = static_cast<float>(i) / 400.0F;
    float const offset = melee_lunge_offset(
        CombatAttackFamily::Sword, phase, MeleeSwingOutcome::Heavy, false);
    EXPECT_LT(std::abs(offset - previous), 0.02F) << "step at phase " << phase;
    previous = offset;
  }
}

TEST(CombatRootMotion, ReactionsRecoilAlongTheBlowAndNeverTopple) {
  for (auto const form : {HitReactionForm::Flinch,
                          HitReactionForm::Block,
                          HitReactionForm::Evade,
                          HitReactionForm::Stagger,
                          HitReactionForm::Recoil}) {
    float peak_back = 0.0F;
    float peak_tilt = 0.0F;
    for (int i = 0; i <= 40; ++i) {
      CombatRootMotionInputs inputs{};
      inputs.hit_reacting = true;
      inputs.reaction = form;
      inputs.reaction_progress = static_cast<float>(i) / 40.0F;
      inputs.reaction_intensity = 1.5F;
      inputs.recoil_dir_x = 0.0F;
      inputs.recoil_dir_z = -1.0F;
      auto const sample = resolve_combat_root_motion(inputs);
      EXPECT_TRUE(sample.active);
      EXPECT_LE(sample.world_offset_z, 1.0e-5F) << "recoil must travel with the blow";
      EXPECT_NEAR(sample.world_offset_x,
                  0.0F,
                  form == HitReactionForm::Evade || form == HitReactionForm::Stagger
                      ? 0.30F
                      : 1.0e-5F);
      peak_back = std::max(peak_back, -sample.world_offset_z);
      peak_tilt = std::max(peak_tilt, std::abs(sample.pitch_degrees));
    }
    EXPECT_GT(peak_back, 0.03F);
    EXPECT_LT(peak_tilt, 16.0F) << "a reaction is a recoil, not a fall";

    CombatRootMotionInputs done{};
    done.hit_reacting = true;
    done.reaction = form;
    done.reaction_progress = 1.0F;
    done.recoil_dir_z = -1.0F;
    auto const settled = resolve_combat_root_motion(done);
    EXPECT_NEAR(settled.world_offset_z, 0.0F, 1.0e-4F);
    EXPECT_NEAR(settled.pitch_degrees, 0.0F, 1.0e-3F);
  }
}

TEST(CombatRootMotion, SimulationDisplacementShrinksTheVisualRecoil) {
  CombatRootMotionInputs visual{};
  visual.hit_reacting = true;
  visual.reaction = HitReactionForm::Flinch;
  visual.reaction_progress = 0.3F;
  visual.recoil_dir_z = -1.0F;
  auto displaced = visual;
  displaced.body_displaced_by_simulation = true;
  EXPECT_LT(std::abs(resolve_combat_root_motion(displaced).world_offset_z),
            std::abs(resolve_combat_root_motion(visual).world_offset_z));
}

TEST(CombatRootMotion, NoMotionWhileIdleOrMounted) {
  CombatRootMotionInputs idle{};
  EXPECT_FALSE(resolve_combat_root_motion(idle).active);

  CombatRootMotionInputs mounted{};
  mounted.attacking = true;
  mounted.melee = true;
  mounted.mounted = true;
  mounted.phase = CombatTransactionPhase::Strike;
  mounted.attack_phase = 0.6F;
  EXPECT_FALSE(resolve_combat_root_motion(mounted).active);
}

TEST(ReactionPose, ReadyStanceLoopsSeamlesslyAndCrouches) {
  auto const start = Animation::resolve_humanoid_ready_stance(
      {.phase = 0.0F, .weapon = Animation::HumanoidReadyWeapon::SwordAndShield});
  auto const end = Animation::resolve_humanoid_ready_stance(
      {.phase = 1.0F, .weapon = Animation::HumanoidReadyWeapon::SwordAndShield});
  EXPECT_NEAR(start.crouch, end.crouch, 1.0e-4F);
  EXPECT_NEAR(start.weapon_phase, end.weapon_phase, 1.0e-4F);
  EXPECT_GT(start.crouch, 0.02F);
  EXPECT_GT(start.shield_raise, 0.3F);
  EXPECT_LT(start.weapon_phase, 0.08F)
      << "the stance must sit on the opening frame of the swing";
}

TEST(ReactionPose, ReactionsStartAndEndAtRest) {
  for (auto const kind : {Animation::HumanoidReactionKind::Flinch,
                          Animation::HumanoidReactionKind::Block,
                          Animation::HumanoidReactionKind::Evade,
                          Animation::HumanoidReactionKind::Stagger}) {
    auto const first = Animation::resolve_humanoid_reaction_pose(
        {.kind = kind, .phase = 0.0F, .weapon = Animation::HumanoidReadyWeapon::Spear});
    auto const last = Animation::resolve_humanoid_reaction_pose(
        {.kind = kind, .phase = 1.0F, .weapon = Animation::HumanoidReadyWeapon::Spear});
    EXPECT_NEAR(first.envelope, 0.0F, 1.0e-4F);
    EXPECT_NEAR(last.envelope, 0.0F, 1.0e-4F);
    EXPECT_NEAR(first.crouch, 0.0F, 1.0e-4F);
    EXPECT_NEAR(last.torso_forward, 0.0F, 1.0e-3F);
    auto const mid = Animation::resolve_humanoid_reaction_pose(
        {.kind = kind, .phase = 0.3F, .weapon = Animation::HumanoidReadyWeapon::Spear});
    EXPECT_GT(mid.envelope, 0.5F);
  }
  auto const block = Animation::resolve_humanoid_reaction_pose(
      {.kind = Animation::HumanoidReactionKind::Block,
       .phase = 0.22F,
       .weapon = Animation::HumanoidReadyWeapon::SwordAndShield});
  EXPECT_GT(block.shield_raise, 0.9F) << "a shielded block raises the shield";
}

} // namespace
