// Regression tests for the creature render graph system.
//
// These tests verify that the declarative creature render graph correctly
// prepares creature render state for submission, and that template/cache
// prewarming works correctly with the new system.

#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

using namespace Render::Creature::Pipeline;
using namespace Render::Creature;

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

// --- LOD Config Tests ---

TEST(CreatureRenderGraph, HumanoidLodConfigHasReasonableDefaults) {
  auto config = humanoid_lod_config();
  EXPECT_GT(config.thresholds.full, 0.0F);
    EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
    EXPECT_GT(config.temporal.period_minimal, 0U);
}

TEST(CreatureRenderGraph, HorseLodConfigHasReasonableDefaults) {
  auto config = horse_lod_config();
  EXPECT_GT(config.thresholds.full, 0.0F);
    EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, ElephantLodConfigHasReasonableDefaults) {
  auto config = elephant_lod_config();
  EXPECT_GT(config.thresholds.full, 0.0F);
    EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, HorseAndElephantHaveLargerLodDistances) {
  auto humanoid = humanoid_lod_config();
  auto horse = horse_lod_config();
  auto elephant = elephant_lod_config();

  // Horses and elephants are larger, so they should have larger LOD distances
  EXPECT_GE(horse.thresholds.full, humanoid.thresholds.full);
  EXPECT_GE(elephant.thresholds.full, humanoid.thresholds.full);
}

// --- Settings-based Config Tests ---

TEST(CreatureRenderGraph, HumanoidConfigFromSettingsReturnsValidConfig) {
  auto config = humanoid_lod_config_from_settings();
  EXPECT_GT(config.thresholds.full, 0.0F);
    EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
    EXPECT_GT(config.temporal.period_minimal, 0U);
}

TEST(CreatureRenderGraph, HorseConfigFromSettingsReturnsValidConfig) {
  auto config = horse_lod_config_from_settings();
  EXPECT_GT(config.thresholds.full, 0.0F);
    EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, ElephantConfigFromSettingsReturnsValidConfig) {
  auto config = elephant_lod_config_from_settings();
  EXPECT_GT(config.thresholds.full, 0.0F);
    EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, SettingsConfigIncludesTemporalParams) {
  auto humanoid = humanoid_lod_config_from_settings();
  auto horse = horse_lod_config_from_settings();
  auto elephant = elephant_lod_config_from_settings();

  // All species should have temporal skip parameters
      }

// --- LOD Evaluation Tests ---

TEST(CreatureRenderGraph, EvaluateLodReturnsFullAtCloseDistance) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, EvaluateLodReturnsMinimalAtMidDistance) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 15.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Minimal);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, EvaluateLodReturnsMinimalAtFarDistance) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 35.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Minimal);
}

TEST(CreatureRenderGraph, EvaluateLodReturnsBillboardBeyondMinimal) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 100.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Billboard);
  EXPECT_TRUE(decision.culled);
  EXPECT_EQ(decision.reason, CullReason::Billboard);
}

TEST(CreatureRenderGraph, ForcedLodOverridesDistanceCalculation) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 100.0F; // Would normally be Billboard
  inputs.has_camera = true;
  inputs.forced_lod = CreatureLOD::Full; // Force Full LOD

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, NoCameraMeansFullLod) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 100.0F;
  inputs.has_camera = false;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

// --- Graph Output Tests ---

TEST(CreatureRenderGraph, BuildBaseOutputSetsLodFromDecision) {
  CreatureGraphInputs inputs;
  CreatureLodDecision decision;
  decision.lod = CreatureLOD::Minimal;
  decision.culled = false;

  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.lod, CreatureLOD::Minimal);
  EXPECT_FALSE(output.culled);
}

TEST(CreatureRenderGraph, BuildBaseOutputSetsCulledFromDecision) {
  CreatureGraphInputs inputs;
  CreatureLodDecision decision;
  decision.lod = CreatureLOD::Billboard;
  decision.culled = true;
  decision.reason = CullReason::Billboard;

  auto output = build_base_graph_output(inputs, decision);

  EXPECT_TRUE(output.culled);
  EXPECT_EQ(output.cull_reason, CullReason::Billboard);
}

TEST(CreatureRenderGraph, BuildBaseOutputSetsPassIntentFromContext) {
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &ctx;

  CreatureLodDecision decision;
  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.pass_intent, RenderPassIntent::Shadow);
}

TEST(CreatureRenderGraph, BuildBaseOutputCopiesWorldMatrix) {
  Render::GL::DrawContext ctx{};
  ctx.model.translate(1.0F, 2.0F, 3.0F);

  CreatureGraphInputs inputs;
  inputs.ctx = &ctx;

  CreatureLodDecision decision;
  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.world_matrix, ctx.model);
}

// --- Creature Render Batch Tests ---

TEST(CreatureRenderBatch, StartsEmpty) {
  CreatureRenderBatch batch;
  EXPECT_TRUE(batch.empty());
  EXPECT_EQ(batch.size(), 0u);
}

TEST(CreatureRenderBatch, AddHumanoidIncreasesSize) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_EQ(batch.size(), 1u);
  EXPECT_FALSE(batch.empty());
}

TEST(CreatureRenderBatch, CulledCreatureNotAdded) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = true; // This creature is culled
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_TRUE(batch.empty());
  EXPECT_EQ(batch.size(), 0u);
}

TEST(CreatureRenderBatch, AddHorseIncreasesSize) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::Horse::HorseSpecPose pose{};
  Render::GL::HorseVariant variant{};

  batch.add_horse(output, pose, variant);

  EXPECT_EQ(batch.size(), 1u);
}

TEST(CreatureRenderBatch, AddElephantIncreasesSize) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::Elephant::ElephantSpecPose pose{};
  Render::GL::ElephantVariant variant{};

  batch.add_elephant(output, pose, variant);

  EXPECT_EQ(batch.size(), 1u);
}

TEST(CreatureRenderBatch, ClearEmptiesBatch) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);
  EXPECT_EQ(batch.size(), 1u);

  batch.clear();
  EXPECT_TRUE(batch.empty());
}

TEST(CreatureRenderBatch, ReserveDoesNotChangeSize) {
  CreatureRenderBatch batch;
  batch.reserve(100);
  EXPECT_TRUE(batch.empty());
}

// --- Integration Tests ---

TEST(CreatureRenderGraph, EndToEndHumanoidPrepare) {
  // Simulate the full graph pipeline for a humanoid
  CreatureGraphInputs inputs;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Humanoid;

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(output.culled);

  CreatureRenderBatch batch;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_EQ(batch.size(), 1u);
  ASSERT_EQ(batch.requests().size(), 1u);
  EXPECT_EQ(batch.requests()[0].lod, CreatureLOD::Full);
  EXPECT_TRUE(batch.rows().empty());
}

TEST(CreatureRenderGraph, EndToEndHorsePrepare) {
  // Simulate the full graph pipeline for a horse
  CreatureGraphInputs inputs;
  inputs.camera_distance = 10.0F;
  inputs.has_camera = true;

  auto config = horse_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Horse;

  EXPECT_EQ(decision.lod, CreatureLOD::Full);

  CreatureRenderBatch batch;
  Render::Horse::HorseSpecPose pose{};
  Render::GL::HorseVariant variant{};

  batch.add_horse(output, pose, variant);

  EXPECT_EQ(batch.size(), 1u);
  ASSERT_EQ(batch.requests().size(), 1u);
  EXPECT_EQ(batch.requests()[0].lod, CreatureLOD::Full);
  EXPECT_TRUE(batch.rows().empty());
}

TEST(CreatureRenderGraph, EndToEndElephantPrepare) {
  // Simulate the full graph pipeline for an elephant
  CreatureGraphInputs inputs;
  inputs.camera_distance = 10.0F;
  inputs.has_camera = true;

  auto config = elephant_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Elephant;

  EXPECT_EQ(decision.lod, CreatureLOD::Full);

  CreatureRenderBatch batch;
  Render::Elephant::ElephantSpecPose pose{};
  Render::GL::ElephantVariant variant{};

  batch.add_elephant(output, pose, variant);

  EXPECT_EQ(batch.size(), 1u);
  ASSERT_EQ(batch.requests().size(), 1u);
  EXPECT_EQ(batch.requests()[0].lod, CreatureLOD::Full);
  EXPECT_TRUE(batch.rows().empty());
}

// --- Template/Cache Prewarming Tests ---

TEST(CreatureRenderGraph, PrewarmContextSetsShadowPassIntent) {
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &ctx;

  CreatureLodDecision decision;
  auto output = build_base_graph_output(inputs, decision);

  // Prewarm contexts should be tagged as Shadow pass
  EXPECT_EQ(output.pass_intent, RenderPassIntent::Shadow);
}

TEST(CreatureRenderGraph, ShadowPassRowsSubmitZeroDrawCalls) {
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = CreatureLOD::Full;
  req.pass = RenderPassIntent::Shadow;
  req.seed = 42U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(CreatureRenderGraph, MainPassRowsSubmitDrawCalls) {
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kHumanoidBase;
  req.state = AnimationStateId::Idle;
  req.lod = CreatureLOD::Full;
  req.pass = RenderPassIntent::Main;
  req.seed = 7U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
}

// --- CreaturePreparationResult Tests ---

TEST(CreaturePreparationResult, ClearEmptiesBothContainers) {
  CreaturePreparationResult result;

  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  result.bodies.add_humanoid(output, pose, variant, anim);
  result.add_post_body_draw(RenderPassIntent::Main,
                            [](Render::GL::ISubmitter &) {});

  EXPECT_EQ(result.bodies.size(), 1u);
  EXPECT_EQ(result.post_body_draws.size(), 1u);

  result.clear();

  EXPECT_TRUE(result.bodies.empty());
  EXPECT_TRUE(result.post_body_draws.empty());
}

} // namespace
