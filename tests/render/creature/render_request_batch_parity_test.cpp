

#include <QMatrix4x4>

#include <gtest/gtest.h>

#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"

namespace {

using Render::Creature::AnimationStateId;
using Render::Creature::ArchetypeRegistry;
using Render::Creature::Pipeline::CreatureGraphOutput;
using Render::Creature::Pipeline::CreatureKind;
using Render::Creature::Pipeline::CreatureRenderBatch;

[[nodiscard]] auto make_output(CreatureKind kind,
                               std::uint32_t seed,
                               float ty) noexcept -> CreatureGraphOutput {
  CreatureGraphOutput out;
  out.spec.kind = kind;
  out.lod = Render::Creature::CreatureLOD::Full;
  out.seed = seed;
  out.entity_id = 0xCAFE0000U + seed;
  out.world_matrix.translate(0.0F, ty, 0.0F);
  return out;
}

TEST(CreatureRenderBatch, RequestMirrorsHumanoidAdd) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 7U, 1.5F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  anim.gait.cycle_phase = 0.42F;

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_TRUE(batch.rows().empty());
  ASSERT_EQ(batch.requests().size(), 1U);

  const auto& req = batch.requests()[0];
  EXPECT_EQ(req.archetype, ArchetypeRegistry::k_humanoid_base);
  EXPECT_EQ(req.state, AnimationStateId::Walk);
  EXPECT_FLOAT_EQ(req.phase, 0.42F);
  EXPECT_EQ(req.entity_id, output.entity_id);
  EXPECT_EQ(req.seed, output.seed);
  EXPECT_EQ(req.world_key,
            (static_cast<Render::Creature::WorldKey>(output.entity_id) << 32U) |
                static_cast<Render::Creature::WorldKey>(output.seed));
  EXPECT_EQ(req.lod, Render::Creature::CreatureLOD::Full);
  EXPECT_EQ(req.pass, Render::Creature::Pipeline::RenderPassIntent::Main);
  EXPECT_TRUE(req.world_already_grounded);
  EXPECT_FLOAT_EQ(req.world.column(3).y(), 1.5F);
}

TEST(CreatureRenderBatch, RequestMirrorsPassIntent) {
  CreatureRenderBatch batch;
  auto output = make_output(CreatureKind::Humanoid, 12U, 0.0F);
  output.pass_intent = Render::Creature::Pipeline::RenderPassIntent::Shadow;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].pass,
            Render::Creature::Pipeline::RenderPassIntent::Shadow);
  ASSERT_EQ(batch.rows().size(), 1U);
  EXPECT_EQ(batch.rows()[0].pass, Render::Creature::Pipeline::RenderPassIntent::Shadow);
}

TEST(CreatureRenderBatch, RiderArchetypeStillUsesAbsoluteRequestWorld) {
  CreatureRenderBatch batch;
  auto output = make_output(CreatureKind::Humanoid, 9U, 3.25F);
  output.spec.archetype_id = ArchetypeRegistry::k_rider_base;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 1U);
  auto const expected_key =
      (static_cast<Render::Creature::WorldKey>(output.entity_id) << 32U) |
      static_cast<Render::Creature::WorldKey>(output.seed);
  EXPECT_EQ(batch.requests()[0].world_key, expected_key);
  EXPECT_FLOAT_EQ(batch.requests()[0].world.column(3).y(), 3.25F);
}

TEST(CreatureRenderBatch, RequestStateForHumanoidHoldAndAttack) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 1U, 0.0F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_in_hold_mode = true;

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::Hold);
}

TEST(CreatureRenderBatch, HoldEntryUsesKneelProgressForRequestPhase) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 4U, 0.0F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_in_hold_mode = true;
  anim.inputs.hold_entry_progress = 0.5F;

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::Hold);
  EXPECT_FLOAT_EQ(batch.requests()[0].phase, 0.5F);
}

TEST(CreatureRenderBatch, HoldExitUsesReverseStandUpProgressForRequestPhase) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 5U, 0.0F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_exiting_hold = true;
  anim.inputs.hold_exit_progress = 0.25F;

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::Hold);
  // hold_transition_amount eases the kneel/stand-up phase with a smoothstep
  // (consistent with guard_pose_amount): linear 0.75 -> smoothstep(0.75) = 0.84375.
  EXPECT_FLOAT_EQ(batch.requests()[0].phase, 0.84375F);
}

TEST(CreatureRenderBatch, FullHoldKeepsTerminalKneelFrame) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 6U, 0.0F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.is_in_hold_mode = true;
  anim.inputs.hold_entry_progress = 1.0F;

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::Hold);
  EXPECT_GT(batch.requests()[0].phase, 0.99F);
  EXPECT_LT(batch.requests()[0].phase, 1.0F);
}

TEST(CreatureRenderBatch, RequestStateForHumanoidMeleeAttack) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 2U, 0.0F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.gait.state = Render::GL::HumanoidMotionState::Attacking;
  anim.attack_phase = 0.33F;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_variant = 1U;

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  const auto& req = batch.requests()[0];
  EXPECT_EQ(req.state, AnimationStateId::AttackSword);
  EXPECT_FLOAT_EQ(req.phase, 0.33F);
  EXPECT_EQ(req.clip_variant, 1U);
}

TEST(CreatureRenderBatch, RequestStateForHumanoidRangedAttack) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 3U, 0.0F);

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.gait.state = Render::GL::HumanoidMotionState::Attacking;
  anim.attack_phase = 0.75F;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = false;

  batch.add_humanoid(output, pose, variant, anim);
  ASSERT_EQ(batch.requests().size(), 1U);
  const auto& req = batch.requests()[0];
  EXPECT_EQ(req.state, AnimationStateId::AttackBow);
  EXPECT_FLOAT_EQ(req.phase, 0.75F);
  EXPECT_EQ(req.clip_variant, 0U);
}

TEST(CreatureRenderBatch, RequestMirrorsHorseQuadrupedState) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Horse, 11U, 0.0F);

  Render::GL::HorseVariant const variant{};

  batch.add_quadruped(output, variant, AnimationStateId::Run, 0.25F);
  batch.add_quadruped(output, variant, AnimationStateId::Walk, 0.0F);
  batch.add_quadruped(output, variant, AnimationStateId::Idle, 0.0F);

  ASSERT_EQ(batch.requests().size(), 3U);
  EXPECT_EQ(batch.requests()[0].archetype, ArchetypeRegistry::k_horse_base);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::Run);
  EXPECT_EQ(batch.requests()[1].state, AnimationStateId::Walk);
  EXPECT_EQ(batch.requests()[2].state, AnimationStateId::Idle);
  EXPECT_FLOAT_EQ(batch.requests()[0].phase, 0.25F);
}

TEST(CreatureRenderBatch, RequestMirrorsElephantQuadrupedState) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Elephant, 3U, 0.0F);

  Render::GL::ElephantVariant const variant{};

  batch.add_quadruped(output, variant, AnimationStateId::Run, 0.7F);
  batch.add_quadruped(output, variant, AnimationStateId::Idle, 0.0F);

  ASSERT_EQ(batch.requests().size(), 2U);
  EXPECT_EQ(batch.requests()[0].archetype, ArchetypeRegistry::k_elephant_base);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::Run);
  EXPECT_EQ(batch.requests()[1].state, AnimationStateId::Idle);
}

TEST(CreatureRenderBatch, CulledOutputDoesNotEmitRequest) {
  CreatureRenderBatch batch;
  auto output = make_output(CreatureKind::Humanoid, 1U, 0.0F);
  output.culled = true;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};
  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_TRUE(batch.rows().empty());
  EXPECT_TRUE(batch.requests().empty());
}

TEST(CreatureRenderBatch, AddRequestAppendsDirectly) {
  CreatureRenderBatch batch;
  Render::Creature::CreatureRenderRequest req;
  req.archetype = ArchetypeRegistry::k_humanoid_base;
  req.state = AnimationStateId::Idle;
  req.entity_id = 0xDEADU;
  batch.add_request(req);

  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.size(), 1U);
  EXPECT_FALSE(batch.empty());
  EXPECT_EQ(batch.requests()[0].entity_id, 0xDEADU);
  EXPECT_TRUE(batch.rows().empty());
}

TEST(CreatureRenderBatch, ClearResetsRequestsAndRows) {
  CreatureRenderBatch batch;
  const auto output = make_output(CreatureKind::Humanoid, 1U, 0.0F);
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};
  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_FALSE(batch.requests().empty());
  batch.clear();
  EXPECT_TRUE(batch.requests().empty());
  EXPECT_TRUE(batch.rows().empty());
}

} // namespace
