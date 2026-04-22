// Phase A/D regression — humanoid prepare module + Shadow pass intent.
//
// Verifies that prepared rows constructed by render/humanoid/prepare.{h,cpp}
// retain the visual_spec/pose/seed they were given, and that submit() of a
// Shadow-tagged row produces zero rigged calls (declarative skip).

#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/formation_calculator.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/prepare.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>
#include <vector>

namespace {

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &) override {
    ++rigged_calls;
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

struct ScopedFlatTerrain {
  explicit ScopedFlatTerrain(float height) {
    auto &terrain = Game::Map::TerrainService::instance();
    std::vector<float> heights(9, height);
    std::vector<Game::Map::TerrainType> terrain_types(
        9, Game::Map::TerrainType::Flat);
    terrain.restore_from_serialized(3, 3, 1.0F, heights, terrain_types, {}, {},
                                    {}, Game::Map::BiomeSettings{});
  }

  ~ScopedFlatTerrain() { Game::Map::TerrainService::instance().clear(); }
};

TEST(HumanoidPrepare, PassIntentFromCtxIsShadowOnPrewarm) {
  Render::GL::DrawContext ctx{};
  EXPECT_EQ(Render::Creature::Pipeline::pass_intent_from_ctx(ctx),
            Render::Creature::Pipeline::RenderPassIntent::Main);
  ctx.template_prewarm = true;
  EXPECT_EQ(Render::Creature::Pipeline::pass_intent_from_ctx(ctx),
            Render::Creature::Pipeline::RenderPassIntent::Shadow);
}

TEST(HumanoidPrepare, ShadowRowSubmitsZeroDrawCalls) {
  using namespace Render::Creature::Pipeline;

  PreparedCreatureRenderRow row{};
  row.spec.kind = CreatureKind::Humanoid;
  row.lod = Render::Creature::CreatureLOD::Full;
  row.pass = RenderPassIntent::Shadow;
  row.seed = 42U;

  CountingSubmitter sink;
  PreparedCreatureSubmitBatch batch;
  batch.add(row);
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(HumanoidPrepare, MainRowStillSubmitsOneRiggedCall) {
  using namespace Render::Creature::Pipeline;

  PreparedCreatureRenderRow row{};
  row.spec.kind = CreatureKind::Humanoid;
  row.lod = Render::Creature::CreatureLOD::Full;
  row.pass = RenderPassIntent::Main;
  row.seed = 7U;

  CountingSubmitter sink;
  PreparedCreatureSubmitBatch batch;
  batch.add(row);
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_GT(sink.rigged_calls + sink.meshes, 0);
}

TEST(HumanoidPrepare, MixedBatchOnlySubmitsMainRows) {
  using namespace Render::Creature::Pipeline;

  CountingSubmitter sink;
  PreparedCreatureSubmitBatch batch;
  for (int i = 0; i < 5; ++i) {
    PreparedCreatureRenderRow row{};
    row.spec.kind = CreatureKind::Humanoid;
    row.lod = Render::Creature::CreatureLOD::Full;
    row.pass = (i % 2 == 0) ? RenderPassIntent::Main : RenderPassIntent::Shadow;
    row.seed = static_cast<std::uint32_t>(i + 1);
    batch.add(row);
  }
  const auto stats = batch.submit(sink);

  // 3 Main rows (idx 0, 2, 4) → exactly 3 entity submissions; Shadow
  // rows are filtered before reaching the pipeline.
  EXPECT_EQ(stats.entities_submitted, 3u);
}

TEST(HumanoidPrepare, DeriveUnitSeedHonorsOverride) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = true;
  ctx.seed_override = 0xDEADBEEFu;
  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr),
            0xDEADBEEFu);
}

TEST(HumanoidPrepare, DeriveUnitSeedDeterministicWithoutOverride) {
  Render::GL::DrawContext ctx{};
  // No entity, no unit, no override → should return 0 deterministically.
  EXPECT_EQ(Render::Creature::Pipeline::derive_unit_seed(ctx, nullptr), 0u);
}

TEST(HumanoidPrepare, BuildSoldierLayoutIsDeterministic) {
  using Render::GL::FormationCalculatorFactory;
  auto const *calculator = FormationCalculatorFactory::get_calculator(
      FormationCalculatorFactory::Nation::Roman,
      FormationCalculatorFactory::UnitCategory::Infantry);
  ASSERT_NE(calculator, nullptr);

  Render::Humanoid::SoldierLayoutInputs inputs{};
  inputs.idx = 3;
  inputs.row = 1;
  inputs.col = 1;
  inputs.rows = 2;
  inputs.cols = 2;
  inputs.formation_spacing = 1.25F;
  inputs.seed = 0x12345678U;
  inputs.force_single_soldier = false;
  inputs.melee_attack = true;
  inputs.animation_time = 2.5F;

  auto const first =
      Render::Humanoid::build_soldier_layout(*calculator, inputs);
  auto const second =
      Render::Humanoid::build_soldier_layout(*calculator, inputs);

  EXPECT_FLOAT_EQ(first.offset_x, second.offset_x);
  EXPECT_FLOAT_EQ(first.offset_z, second.offset_z);
  EXPECT_FLOAT_EQ(first.vertical_jitter, second.vertical_jitter);
  EXPECT_FLOAT_EQ(first.yaw_offset, second.yaw_offset);
  EXPECT_FLOAT_EQ(first.phase_offset, second.phase_offset);
  EXPECT_EQ(first.inst_seed, second.inst_seed);
}

TEST(HumanoidPrepare, BuildSoldierLayoutLeavesSingleSoldierUnjittered) {
  using Render::GL::FormationCalculatorFactory;
  auto const *calculator = FormationCalculatorFactory::get_calculator(
      FormationCalculatorFactory::Nation::Roman,
      FormationCalculatorFactory::UnitCategory::Infantry);
  ASSERT_NE(calculator, nullptr);

  Render::Humanoid::SoldierLayoutInputs inputs{};
  inputs.idx = 0;
  inputs.row = 0;
  inputs.col = 0;
  inputs.rows = 1;
  inputs.cols = 1;
  inputs.formation_spacing = 1.0F;
  inputs.seed = 0xCAFEBABEU;
  inputs.force_single_soldier = true;
  inputs.melee_attack = false;
  inputs.animation_time = 0.0F;

  auto const layout =
      Render::Humanoid::build_soldier_layout(*calculator, inputs);

  EXPECT_FLOAT_EQ(layout.offset_x, 0.0F);
  EXPECT_FLOAT_EQ(layout.offset_z, 0.0F);
  EXPECT_FLOAT_EQ(layout.vertical_jitter, 0.0F);
  EXPECT_FLOAT_EQ(layout.yaw_offset, 0.0F);
  EXPECT_FLOAT_EQ(layout.phase_offset, 0.0F);
  EXPECT_EQ(layout.inst_seed, inputs.seed);
}

TEST(HumanoidPrepare, BuildAmbientIdleStateIsDeterministicPerUnit) {
  Render::GL::AnimationInputs anim{};
  anim.time = 2.5F;
  anim.is_moving = false;
  anim.is_attacking = false;

  auto const first = Render::Humanoid::build_humanoid_ambient_idle_state(
      anim, 0x1234ABCDU, 4, anim.time);
  auto const second = Render::Humanoid::build_humanoid_ambient_idle_state(
      anim, 0x1234ABCDU, 4, anim.time);

  EXPECT_EQ(first.idle_type, second.idle_type);
  EXPECT_FLOAT_EQ(first.phase, second.phase);
  EXPECT_EQ(first.primary_index, second.primary_index);
  EXPECT_EQ(first.secondary_index, second.secondary_index);
  EXPECT_GE(first.primary_index, 0);
  EXPECT_LT(first.primary_index, 4);
  if (first.secondary_index >= 0) {
    EXPECT_LT(first.secondary_index, 4);
    EXPECT_NE(first.secondary_index, first.primary_index);
  }
  EXPECT_TRUE(Render::Humanoid::is_humanoid_ambient_idle_active(
      first, first.primary_index));
}

TEST(HumanoidPrepare, BuildAmbientIdleStateDisablesOutsideIdleWindow) {
  Render::GL::AnimationInputs anim{};
  anim.time = 7.5F;
  anim.is_moving = false;
  anim.is_attacking = false;

  auto const state = Render::Humanoid::build_humanoid_ambient_idle_state(
      anim, 0xABCD1234U, 5, anim.time);

  EXPECT_EQ(state.idle_type, Render::GL::AmbientIdleType::None);
  EXPECT_FLOAT_EQ(state.phase, 0.0F);
  EXPECT_EQ(state.primary_index, -1);
  EXPECT_EQ(state.secondary_index, -1);
  EXPECT_FALSE(Render::Humanoid::is_humanoid_ambient_idle_active(state, 0));
}

TEST(HumanoidPrepare, BuildAmbientIdleStateDisablesDuringMovement) {
  Render::GL::AnimationInputs anim{};
  anim.time = 2.0F;
  anim.is_moving = true;
  anim.is_attacking = false;

  auto const state = Render::Humanoid::build_humanoid_ambient_idle_state(
      anim, 0xABCD1234U, 5, anim.time);

  EXPECT_EQ(state.idle_type, Render::GL::AmbientIdleType::None);
  EXPECT_EQ(state.primary_index, -1);
  EXPECT_EQ(state.secondary_index, -1);
}

TEST(HumanoidPrepare, BuildLocomotionStateIsDeterministicForRun) {
  Render::Humanoid::HumanoidLocomotionInputs inputs{};
  inputs.anim.time = 1.5F;
  inputs.anim.is_moving = true;
  inputs.anim.is_running = true;
  inputs.variation.walk_speed_mult = 1.10F;
  inputs.move_speed = 4.8F;
  inputs.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  inputs.movement_target = QVector3D(3.0F, 0.0F, 8.0F);
  inputs.has_movement_target = true;
  inputs.animation_time = 1.5F;
  inputs.phase_offset = 0.125F;

  auto const first = Render::Humanoid::build_humanoid_locomotion_state(inputs);
  auto const second = Render::Humanoid::build_humanoid_locomotion_state(inputs);

  EXPECT_EQ(first.motion_state, Render::GL::HumanoidMotionState::Run);
  EXPECT_EQ(first.motion_state, second.motion_state);
  EXPECT_FLOAT_EQ(first.gait.cycle_time, second.gait.cycle_time);
  EXPECT_FLOAT_EQ(first.gait.cycle_phase, second.gait.cycle_phase);
  EXPECT_FLOAT_EQ(first.gait.normalized_speed, second.gait.normalized_speed);
  EXPECT_FLOAT_EQ(first.gait.stride_distance, second.gait.stride_distance);
  EXPECT_EQ(first.has_movement_target, second.has_movement_target);
  EXPECT_GT(first.gait.cycle_time, 0.0F);
  EXPECT_GT(first.gait.stride_distance, 0.0F);
}

TEST(HumanoidPrepare, RunPoseShapingAppliesExplicitOffsets) {
  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.motion_state = Render::GL::HumanoidMotionState::Run;
  anim_ctx.gait.normalized_speed = 0.85F;
  anim_ctx.locomotion_phase = 0.30F;
  anim_ctx.entity_forward = QVector3D(0.0F, 0.0F, 1.0F);
  anim_ctx.variation.height_scale = 1.0F;

  auto const shaping =
      Render::Humanoid::build_humanoid_run_pose_shaping(anim_ctx);

  Render::GL::HumanoidPose pose{};
  pose.pelvis_pos = QVector3D(0.0F, 1.0F, 0.0F);
  pose.shoulder_l = QVector3D(-0.20F, 1.55F, 0.0F);
  pose.shoulder_r = QVector3D(0.20F, 1.55F, 0.0F);
  pose.neck_base = QVector3D(0.0F, 1.62F, 0.0F);
  pose.head_pos = QVector3D(0.0F, 1.78F, 0.0F);
  pose.hand_l = QVector3D(-0.14F, 1.05F, 0.02F);
  pose.hand_r = QVector3D(0.14F, 1.05F, -0.02F);
  pose.foot_l = QVector3D(-0.12F, 0.02F, 0.08F);
  pose.foot_r = QVector3D(0.12F, 0.02F, -0.08F);
  pose.head_frame.radius = 0.10F;
  pose.head_frame.up = QVector3D(0.0F, 1.0F, 0.0F);
  pose.head_frame.right = QVector3D(1.0F, 0.0F, 0.0F);
  pose.head_frame.forward = QVector3D(0.0F, 0.0F, 1.0F);

  Render::Humanoid::apply_humanoid_run_pose_shaping(pose, anim_ctx, shaping);

  EXPECT_LT(pose.pelvis_pos.z(), 0.0F);
  EXPECT_LT(pose.pelvis_pos.y(), 1.0F);
  EXPECT_GT(pose.shoulder_l.z(), 0.0F);
  EXPECT_GT(pose.shoulder_r.z(), 0.0F);
  EXPECT_NE(pose.hand_l.z(), 0.02F);
  EXPECT_NE(pose.hand_r.z(), -0.02F);
}

TEST(HumanoidPrepare, PreparedSingleSoldierSnapsToTerrainHeight) {
  ScopedFlatTerrain terrain(2.75F);

  Render::GL::HumanoidRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.model.translate(0.4F, 8.0F, -0.2F);
  ctx.force_single_soldier = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;
  anim.is_moving = false;
  anim.is_running = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.combat_phase = Render::GL::CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_variant = 0;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_healing = false;
  anim.healing_target_dx = 0.0F;
  anim.healing_target_dz = 0.0F;
  anim.is_constructing = false;
  anim.construction_progress = 0.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(owner, ctx, anim, 1U, prep);

  auto const rows = prep.bodies.rows();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_NEAR(rows[0].world_from_unit.map(QVector3D(0.0F, 0.0F, 0.0F)).y(),
              2.75F, 0.0001F);
}

} // namespace
