#include "render/creature/pipeline/shadow_batch.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

using namespace Render::Creature::Pipeline;

// Minimal ISubmitter stub – not a real Renderer, so flush_shadow_batch's
// dynamic_cast<Renderer*> returns nullptr and shadows are not drawn.
class StubSubmitter : public Render::GL::ISubmitter {
public:
  int mesh_calls{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &) override {}
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

// ---------------------------------------------------------------------------
// HumanoidShadowBatch structural tests
// ---------------------------------------------------------------------------

TEST(HumanoidShadowBatch, StartsEmpty) {
  HumanoidShadowBatch batch;
  EXPECT_TRUE(batch.empty());
  EXPECT_EQ(batch.size(), 0u);
  EXPECT_EQ(batch.shader(), nullptr);
  EXPECT_EQ(batch.mesh(), nullptr);
}

TEST(HumanoidShadowBatch, AddIncreasesSize) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.5F);
  EXPECT_FALSE(batch.empty());
  EXPECT_EQ(batch.size(), 1u);
}

TEST(HumanoidShadowBatch, AddStoresInstanceData) {
  HumanoidShadowBatch batch;
  QMatrix4x4 model;
  model.translate(1.0F, 2.0F, 3.0F);
  constexpr float alpha = 0.24F;
  batch.add(model, alpha, RenderPassIntent::Main);

  ASSERT_EQ(batch.instances().size(), 1u);
  const auto &inst = batch.instances().front();
  EXPECT_EQ(inst.model, model);
  EXPECT_FLOAT_EQ(inst.alpha, alpha);
  EXPECT_EQ(inst.pass, RenderPassIntent::Main);
}

TEST(HumanoidShadowBatch, AddMultipleInstances) {
  HumanoidShadowBatch batch;
  for (int i = 0; i < 5; ++i) {
    batch.add(QMatrix4x4{}, 0.1F * static_cast<float>(i));
  }
  EXPECT_EQ(batch.size(), 5u);
}

TEST(HumanoidShadowBatch, ClearResetsAll) {
  HumanoidShadowBatch batch;
  batch.init(nullptr, nullptr, QVector2D(1.0F, 0.0F));
  batch.add(QMatrix4x4{}, 0.5F);
  EXPECT_FALSE(batch.empty());

  batch.clear();

  EXPECT_TRUE(batch.empty());
  EXPECT_EQ(batch.size(), 0u);
  EXPECT_EQ(batch.shader(), nullptr);
  EXPECT_EQ(batch.mesh(), nullptr);
}

TEST(HumanoidShadowBatch, InitStoresSharedState) {
  HumanoidShadowBatch batch;
  QVector2D dir(0.5F, 0.8F);
  batch.init(nullptr, nullptr, dir);
  EXPECT_EQ(batch.light_dir(), dir);
}

TEST(HumanoidShadowBatch, DefaultPassIsMain) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.3F);
  ASSERT_EQ(batch.instances().size(), 1u);
  EXPECT_EQ(batch.instances().front().pass, RenderPassIntent::Main);
}

TEST(HumanoidShadowBatch, ShadowPassStoredCorrectly) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.3F, RenderPassIntent::Shadow);
  ASSERT_EQ(batch.instances().size(), 1u);
  EXPECT_EQ(batch.instances().front().pass, RenderPassIntent::Shadow);
}

// ---------------------------------------------------------------------------
// flush_shadow_batch tests
// ---------------------------------------------------------------------------

TEST(FlushShadowBatch, EmptyBatchDoesNotCrash) {
  HumanoidShadowBatch batch;
  StubSubmitter sink;
  flush_shadow_batch(batch, sink);
  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(FlushShadowBatch, NullShaderIsNoOp) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.5F);
  // shader is null → flush should be a safe no-op
  StubSubmitter sink;
  flush_shadow_batch(batch, sink);
  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(FlushShadowBatch, NonRendererSubmitterIsNoOp) {
  HumanoidShadowBatch batch;
  batch.init(nullptr, nullptr, QVector2D(1.0F, 0.0F));
  batch.add(QMatrix4x4{}, 0.5F);

  StubSubmitter sink; // not a Render::GL::Renderer → dynamic_cast fails
  flush_shadow_batch(batch, sink);
  // No crash, no draw calls issued on a non-Renderer submitter
  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(FlushShadowBatch, BatchRemainsIntactAfterFlush) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.5F);
  EXPECT_EQ(batch.size(), 1u);

  StubSubmitter sink;
  flush_shadow_batch(batch, sink);

  // flush does not clear the batch; caller owns the lifetime
  EXPECT_EQ(batch.size(), 1u);
}

} // namespace
