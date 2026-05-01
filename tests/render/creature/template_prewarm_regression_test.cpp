

#include "render/creature/archetype_registry.h"
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
#include <vector>

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

auto make_request(Render::Creature::ArchetypeId archetype, CreatureLOD lod,
                  RenderPassIntent pass,
                  std::uint32_t seed = 0U) -> CreatureRenderRequest {
  CreatureRenderRequest req{};
  req.archetype = archetype;
  req.state = AnimationStateId::Idle;
  req.lod = lod;
  req.pass = pass;
  req.seed = seed;
  req.world_already_grounded = true;
  req.world.translate(0.0F, static_cast<float>(seed) * 0.1F, 0.0F);
  return req;
}

auto submit_requests_for_test(std::span<const CreatureRenderRequest> requests,
                              PrewarmCountingSubmitter &sink) -> SubmitStats {
  CreaturePreparationResult prep;
  prep.bodies.reserve(requests.size());
  for (const auto &request : requests) {
    prep.bodies.add_request(request);
  }
  return submit_preparation(prep, sink);
}

TEST(TemplatePrewarmRegression, PassIntentFromCtxDetectsPrewarm) {
  Render::GL::DrawContext normal_ctx{};
  normal_ctx.template_prewarm = false;
  EXPECT_EQ(pass_intent_from_ctx(normal_ctx), RenderPassIntent::Main);

  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;
  EXPECT_EQ(pass_intent_from_ctx(prewarm_ctx), RenderPassIntent::Shadow);
}

TEST(TemplatePrewarmRegression, ShadowPassFiltersPreparedBatch) {
  std::vector<CreatureRenderRequest> requests;
  for (int i = 0; i < 5; ++i) {
    requests.push_back(make_request(
        ArchetypeRegistry::kHumanoidBase, CreatureLOD::Full,
        (i < 3) ? RenderPassIntent::Main : RenderPassIntent::Shadow,
        static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 3u);
}

TEST(TemplatePrewarmRegression, AllShadowPassRowsProduceZeroDraws) {
  std::vector<CreatureRenderRequest> requests;
  for (int i = 0; i < 10; ++i) {
    requests.push_back(make_request(ArchetypeRegistry::kHumanoidBase,
                                    CreatureLOD::Full, RenderPassIntent::Shadow,
                                    static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.mesh_calls, 0);
}

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

  CreatureRenderBatch batch;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);

  CreaturePreparationResult prep;
  for (const auto &request : batch.requests()) {
    prep.bodies.add_request(request);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(TemplatePrewarmRegression, MixedNormalAndPrewarmBatchFiltersCorrectly) {

  Render::GL::DrawContext normal_ctx{};
  normal_ctx.template_prewarm = false;

  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  std::vector<CreatureRenderRequest> requests;
  for (int i = 0; i < 10; ++i) {
    requests.push_back(
        make_request(ArchetypeRegistry::kHumanoidBase, CreatureLOD::Full,
                     (i % 2 == 0) ? pass_intent_from_ctx(normal_ctx)
                                  : pass_intent_from_ctx(prewarm_ctx),
                     static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 5u);
}

TEST(TemplatePrewarmRegression, HorsePrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PrewarmCountingSubmitter sink;
  auto const request =
      make_request(ArchetypeRegistry::kHorseBase, CreatureLOD::Full,
                   pass_intent_from_ctx(prewarm_ctx), 123U);
  const auto stats = submit_requests_for_test(
      std::span<const CreatureRenderRequest>(&request, 1u), sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(TemplatePrewarmRegression, ElephantPrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PrewarmCountingSubmitter sink;
  auto const request =
      make_request(ArchetypeRegistry::kElephantBase, CreatureLOD::Full,
                   pass_intent_from_ctx(prewarm_ctx), 456U);
  const auto stats = submit_requests_for_test(
      std::span<const CreatureRenderRequest>(&request, 1u), sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(TemplatePrewarmRegression, AllLodLevelsRespectPrewarmFiltering) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  std::vector<CreatureRenderRequest> requests;
  for (auto lod : {CreatureLOD::Full, CreatureLOD::Minimal}) {
    requests.push_back(make_request(ArchetypeRegistry::kHumanoidBase, lod,
                                    pass_intent_from_ctx(prewarm_ctx),
                                    static_cast<std::uint32_t>(lod)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.lod_minimal, 0u);
}

TEST(TemplatePrewarmRegression, SeedDerivationIsDeterministic) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = false;

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

TEST(TemplatePrewarmRegression, LodStatsNotIncrementedForShadowRows) {
  std::vector<CreatureRenderRequest> requests;
  requests.push_back(make_request(ArchetypeRegistry::kHumanoidBase,
                                  CreatureLOD::Full, RenderPassIntent::Shadow));
  requests.push_back(make_request(ArchetypeRegistry::kHorseBase,
                                  CreatureLOD::Minimal,
                                  RenderPassIntent::Shadow));

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.lod_minimal, 0u);
}

TEST(TemplatePrewarmRegression, MainRowsIncrementLodStats) {
  std::vector<CreatureRenderRequest> requests;
  requests.push_back(make_request(ArchetypeRegistry::kHumanoidBase,
                                  CreatureLOD::Full, RenderPassIntent::Main));
  requests.push_back(make_request(ArchetypeRegistry::kHorseBase,
                                  CreatureLOD::Minimal,
                                  RenderPassIntent::Main));

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
}

} // namespace
