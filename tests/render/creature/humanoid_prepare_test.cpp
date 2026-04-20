// Phase A/D regression — humanoid prepare module + Shadow pass intent.
//
// Verifies that prepared rows constructed by render/humanoid/prepare.{h,cpp}
// retain the visual_spec/pose/seed they were given, and that submit() of a
// Shadow-tagged row produces zero rigged calls (declarative skip).

#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/prepare.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &) override { ++rigged_calls; }
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
    row.pass = (i % 2 == 0) ? RenderPassIntent::Main
                            : RenderPassIntent::Shadow;
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

} // namespace
