// Regression tests for template/cache prewarming with creature rendering.
//
// These tests verify that the declarative creature rendering pipeline
// correctly handles template prewarming scenarios where creatures need to
// be pre-rendered for caching but should not produce actual draw calls.

#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/preparation_common.h"
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

class PrewarmCountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int mesh_calls{0};
  int total_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
    ++total_calls;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &) override {
    ++rigged_calls;
    ++total_calls;
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {
    ++total_calls;
  }
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {
    ++total_calls;
  }
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {
    ++total_calls;
  }
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {
    ++total_calls;
  }
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {
    ++total_calls;
  }
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {
    ++total_calls;
  }
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {
    ++total_calls;
  }
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {
    ++total_calls;
  }
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {
    ++total_calls;
  }
};

// --- Pass Intent Tests ---

TEST(TemplatePrewarmRegression, PassIntentFromCtxDetectsPrewarm) {
  Render::GL::DrawContext normal_ctx{};
  normal_ctx.template_prewarm = false;
  EXPECT_EQ(pass_intent_from_ctx(normal_ctx), RenderPassIntent::Main);

  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;
  EXPECT_EQ(pass_intent_from_ctx(prewarm_ctx), RenderPassIntent::Shadow);
}

TEST(TemplatePrewarmRegression, ShadowPassFiltersPreparedBatch) {
  PreparedCreatureSubmitBatch batch;

  // Add 3 Main pass rows and 2 Shadow pass rows
  for (int i = 0; i < 5; ++i) {
    PreparedCreatureRenderRow row{};
    row.spec.kind = CreatureKind::Humanoid;
    row.lod = CreatureLOD::Full;
    row.pass = (i < 3) ? RenderPassIntent::Main : RenderPassIntent::Shadow;
    row.seed = static_cast<std::uint32_t>(i);
    batch.add(row);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  // Only the 3 Main pass rows should be submitted
  EXPECT_EQ(stats.entities_submitted, 3u);
}

TEST(TemplatePrewarmRegression, AllShadowPassRowsProduceZeroDraws) {
  PreparedCreatureSubmitBatch batch;

  // Add 10 Shadow pass rows
  for (int i = 0; i < 10; ++i) {
    PreparedCreatureRenderRow row{};
    row.spec.kind = CreatureKind::Humanoid;
    row.lod = CreatureLOD::Full;
    row.pass = RenderPassIntent::Shadow;
    row.seed = static_cast<std::uint32_t>(i);
    batch.add(row);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.mesh_calls, 0);
}

// --- Graph Integration Tests ---

TEST(TemplatePrewarmRegression, GraphOutputReflectsPrewarmContext) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;
  prewarm_ctx.model.translate(5.0F, 0.0F, 10.0F);

  CreatureGraphInputs inputs;
  inputs.ctx = &prewarm_ctx;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);

  // Even though LOD is Full, the pass intent should be Shadow
  EXPECT_EQ(output.lod, CreatureLOD::Full);
  EXPECT_EQ(output.pass_intent, RenderPassIntent::Shadow);
}

TEST(TemplatePrewarmRegression, BatchFromPrewarmContextSubmitsNothing) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &prewarm_ctx;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Humanoid;

  // Build a batch from prewarm context
  CreatureRenderBatch batch;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);

  // Convert to submission batch
  PreparedCreatureSubmitBatch submit_batch;
  for (const auto &row : batch.rows()) {
    submit_batch.add(row);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_batch.submit(sink);

  // Shadow-tagged row should produce zero submissions
  EXPECT_EQ(stats.entities_submitted, 0u);
}

// --- Mixed Batch Tests ---

TEST(TemplatePrewarmRegression, MixedNormalAndPrewarmBatchFiltersCorrectly) {
  // Simulate a scenario where normal and prewarm contexts are mixed
  Render::GL::DrawContext normal_ctx{};
  normal_ctx.template_prewarm = false;

  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PreparedCreatureSubmitBatch batch;

  // Add rows alternating between Normal and Prewarm contexts
  for (int i = 0; i < 10; ++i) {
    PreparedCreatureRenderRow row{};
    row.spec.kind = CreatureKind::Humanoid;
    row.lod = CreatureLOD::Full;
    row.pass = (i % 2 == 0) ? RenderPassIntent::Main : RenderPassIntent::Shadow;
    row.seed = static_cast<std::uint32_t>(i);
    batch.add(row);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  // Only even indices (0, 2, 4, 6, 8) = 5 Main rows should be submitted
  EXPECT_EQ(stats.entities_submitted, 5u);
}

// --- Species-Specific Prewarm Tests ---

TEST(TemplatePrewarmRegression, HorsePrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PreparedCreatureRenderRow row{};
  row.spec.kind = CreatureKind::Horse;
  row.lod = CreatureLOD::Full;
  row.pass = pass_intent_from_ctx(prewarm_ctx);
  row.seed = 123U;

  PreparedCreatureSubmitBatch batch;
  batch.add(row);

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(TemplatePrewarmRegression, ElephantPrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PreparedCreatureRenderRow row{};
  row.spec.kind = CreatureKind::Elephant;
  row.lod = CreatureLOD::Full;
  row.pass = pass_intent_from_ctx(prewarm_ctx);
  row.seed = 456U;

  PreparedCreatureSubmitBatch batch;
  batch.add(row);

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

// --- LOD-Independent Prewarm Tests ---

TEST(TemplatePrewarmRegression, AllLodLevelsRespectPrewarmFiltering) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PreparedCreatureSubmitBatch batch;

  // Test all LOD levels
  for (auto lod :
       {CreatureLOD::Full, CreatureLOD::Reduced, CreatureLOD::Minimal}) {
    PreparedCreatureRenderRow row{};
    row.spec.kind = CreatureKind::Humanoid;
    row.lod = lod;
    row.pass = pass_intent_from_ctx(prewarm_ctx);
    row.seed = static_cast<std::uint32_t>(lod);
    batch.add(row);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  // All LOD levels should be filtered when prewarm
  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.lod_reduced, 0u);
  EXPECT_EQ(stats.lod_minimal, 0u);
}

// --- Cache Warming Determinism Tests ---

TEST(TemplatePrewarmRegression, SeedDerivationIsDeterministic) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = false;

  // Call derive_unit_seed multiple times with same inputs
  auto seed1 = derive_unit_seed(ctx, nullptr);
  auto seed2 = derive_unit_seed(ctx, nullptr);
  auto seed3 = derive_unit_seed(ctx, nullptr);

  EXPECT_EQ(seed1, seed2);
  EXPECT_EQ(seed2, seed3);
}

TEST(TemplatePrewarmRegression, SeedOverrideRespected) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = true;
  ctx.seed_override = 0xCAFEBABEU;

  auto seed = derive_unit_seed(ctx, nullptr);
  EXPECT_EQ(seed, 0xCAFEBABEU);
}

// --- Stats Tracking Tests ---

TEST(TemplatePrewarmRegression, LodStatsNotIncrementedForShadowRows) {
  PreparedCreatureSubmitBatch batch;

  // Add Shadow-tagged rows at different LOD levels
  PreparedCreatureRenderRow full_row{};
  full_row.spec.kind = CreatureKind::Humanoid;
  full_row.lod = CreatureLOD::Full;
  full_row.pass = RenderPassIntent::Shadow;
  batch.add(full_row);

  PreparedCreatureRenderRow reduced_row{};
  reduced_row.spec.kind = CreatureKind::Horse;
  reduced_row.lod = CreatureLOD::Reduced;
  reduced_row.pass = RenderPassIntent::Shadow;
  batch.add(reduced_row);

  PreparedCreatureRenderRow minimal_row{};
  minimal_row.spec.kind = CreatureKind::Elephant;
  minimal_row.lod = CreatureLOD::Minimal;
  minimal_row.pass = RenderPassIntent::Shadow;
  batch.add(minimal_row);

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  // Shadow rows should not count toward LOD stats
  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.lod_reduced, 0u);
  EXPECT_EQ(stats.lod_minimal, 0u);
}

TEST(TemplatePrewarmRegression, MainRowsIncrementLodStats) {
  PreparedCreatureSubmitBatch batch;

  PreparedCreatureRenderRow full_row{};
  full_row.spec.kind = CreatureKind::Humanoid;
  full_row.lod = CreatureLOD::Full;
  full_row.pass = RenderPassIntent::Main;
  batch.add(full_row);

  PreparedCreatureRenderRow reduced_row{};
  reduced_row.spec.kind = CreatureKind::Horse;
  reduced_row.lod = CreatureLOD::Reduced;
  reduced_row.pass = RenderPassIntent::Main;
  batch.add(reduced_row);

  PrewarmCountingSubmitter sink;
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);
}

} // namespace
