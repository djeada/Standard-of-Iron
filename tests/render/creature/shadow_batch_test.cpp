#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "render/creature/gpu/rigged_mesh_cache.h"
#include "render/creature/gpu/snapshot_mesh_cache.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_submission_context.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/shadow_batch.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/submitter.h"
#include "render/frame/unseen_submitter.h"
#include "render/scene/scene_renderer.h"

namespace {

using namespace Render::Creature::Pipeline;

class StubSubmitter : public Render::GL::ISubmitter {
public:
  int mesh_calls{0};
  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++mesh_calls;
  }
  void rigged(const Render::GL::RiggedCreatureCmd&) override {}
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

struct RecordedShadowDraw {
  Render::GL::Mesh* mesh{nullptr};
  Render::GL::Shader* shader{nullptr};
  QMatrix4x4 model;
  float alpha{0.0F};
};

class ShaderRecordingRenderer final : public Render::GL::Renderer {
public:
  ShaderRecordingRenderer()
      : Renderer(Render::ShaderQuality::None) {}

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D&,
            Render::GL::Texture*,
            float alpha,
            int) override {
    draws.push_back({mesh, get_current_shader(), model, alpha});
  }

  std::vector<RecordedShadowDraw> draws;
};

class DetachedContextSubmitter final : public StubSubmitter {
public:
  [[nodiscard]] auto
  creature_submission() noexcept -> CreatureSubmissionContext* override {
    return &context;
  }
  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D&,
            Render::GL::Texture*,
            float alpha,
            int) override {
    ++mesh_calls;
    draws.push_back({mesh, current_shader, model, alpha});
  }

  Render::GL::RiggedMeshCache rigged;
  Render::GL::SnapshotMeshCache snapshots;
  Render::GL::Shader* current_shader{nullptr};
  CreatureSubmissionContext context{rigged, snapshots, current_shader};
  std::vector<RecordedShadowDraw> draws;
};

class ScenePassProbeSubmitter final : public Render::GL::ForwardingSubmitter {
public:
  using ForwardingSubmitter::ForwardingSubmitter;
};

auto fake_shader(std::uintptr_t tag) -> Render::GL::Shader* {
  return reinterpret_cast<Render::GL::Shader*>(tag);
}

auto fake_mesh(std::uintptr_t tag) -> Render::GL::Mesh* {
  return reinterpret_cast<Render::GL::Mesh*>(tag);
}

auto shadow_model(float x) -> QMatrix4x4 {
  QMatrix4x4 model;
  model.translate(x, 0.0F, 0.0F);
  return model;
}

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
  const auto& inst = batch.instances().front();
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

TEST(FlushShadowBatch, EmptyBatchDoesNotCrash) {
  HumanoidShadowBatch batch;
  StubSubmitter sink;
  flush_shadow_batch(batch, sink);
  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(FlushShadowBatch, NullShaderIsNoOp) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.5F);

  StubSubmitter sink;
  flush_shadow_batch(batch, sink);
  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(FlushShadowBatch, NonRendererSubmitterIsNoOp) {
  HumanoidShadowBatch batch;
  batch.init(nullptr, nullptr, QVector2D(1.0F, 0.0F));
  batch.add(QMatrix4x4{}, 0.5F);

  StubSubmitter sink;
  flush_shadow_batch(batch, sink);

  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(FlushShadowBatch, BatchRemainsIntactAfterFlush) {
  HumanoidShadowBatch batch;
  batch.add(QMatrix4x4{}, 0.5F);
  EXPECT_EQ(batch.size(), 1u);

  StubSubmitter sink;
  flush_shadow_batch(batch, sink);

  EXPECT_EQ(batch.size(), 1u);
}

TEST(FlushShadowBatch, RendererSinkDrawsWithBatchShaderAndRestoresPrevious) {
  Render::GL::Shader* const previous = fake_shader(0x100);
  Render::GL::Shader* const shadow = fake_shader(0x200);
  Render::GL::Mesh* const quad = fake_mesh(0x300);

  HumanoidShadowBatch batch;
  batch.init(shadow, quad, QVector2D(0.0F, 1.0F));
  batch.add(shadow_model(1.0F), 0.4F);

  ShaderRecordingRenderer renderer;
  renderer.set_current_shader(previous);
  flush_shadow_batch(batch, renderer);

  ASSERT_EQ(renderer.draws.size(), 1U);
  EXPECT_EQ(renderer.draws.front().mesh, quad);
  EXPECT_EQ(renderer.draws.front().shader, shadow);
  EXPECT_EQ(renderer.get_current_shader(), previous);
}

TEST(FlushShadowBatch, WrappedRendererSinkStillDrawsShadows) {
  Render::GL::Shader* const previous = fake_shader(0x100);
  Render::GL::Shader* const shadow = fake_shader(0x200);
  Render::GL::Mesh* const quad = fake_mesh(0x300);

  HumanoidShadowBatch batch;
  batch.init(shadow, quad, QVector2D(0.0F, 1.0F));
  batch.add(shadow_model(1.0F), 0.4F);
  batch.add(shadow_model(2.0F), 0.3F, RenderPassIntent::Shadow);
  batch.add(shadow_model(3.0F), 0.2F);

  ShaderRecordingRenderer renderer;
  renderer.set_current_shader(previous);
  Render::GL::PrimitiveBatcher batcher;
  Render::GL::BatchingSubmitter batching(&renderer, &batcher);
  Render::GL::UnseenSubmitter unseen(batching);
  ScenePassProbeSubmitter probe(unseen);
  ASSERT_EQ(probe.unwrap_submitter(), &renderer);

  flush_shadow_batch(batch, probe);

  ASSERT_EQ(renderer.draws.size(), 2U);
  EXPECT_EQ(renderer.draws[0].mesh, quad);
  EXPECT_EQ(renderer.draws[0].shader, shadow);
  EXPECT_EQ(renderer.draws[0].model, shadow_model(1.0F));
  EXPECT_FLOAT_EQ(renderer.draws[0].alpha, 0.4F);
  EXPECT_EQ(renderer.draws[1].mesh, quad);
  EXPECT_EQ(renderer.draws[1].shader, shadow);
  EXPECT_EQ(renderer.draws[1].model, shadow_model(3.0F));
  EXPECT_FLOAT_EQ(renderer.draws[1].alpha, 0.2F);
  EXPECT_EQ(batcher.total_count(), 0U);
  EXPECT_EQ(renderer.get_current_shader(), previous);
}

TEST(FlushShadowBatch, WrapperWithoutRendererIsNoOp) {
  HumanoidShadowBatch batch;
  batch.init(fake_shader(0x200), fake_mesh(0x300), QVector2D(0.0F, 1.0F));
  batch.add(shadow_model(1.0F), 0.4F);

  StubSubmitter sink;
  ScenePassProbeSubmitter probe(sink);
  flush_shadow_batch(batch, probe);

  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(SubmitPreparation, ShadowBatchReachesRendererThroughScenePassWrappers) {
  Render::GL::Shader* const previous = fake_shader(0x100);
  Render::GL::Shader* const shadow = fake_shader(0x200);
  Render::GL::Mesh* const quad = fake_mesh(0x300);

  CreaturePreparationResult prep;
  prep.shadow_batch.init(shadow, quad, QVector2D(0.0F, 1.0F));
  prep.shadow_batch.add(shadow_model(4.0F), 0.35F);

  ShaderRecordingRenderer renderer;
  renderer.set_current_shader(previous);
  Render::GL::UnseenSubmitter unseen(renderer);
  ScenePassProbeSubmitter probe(unseen);

  (void)submit_preparation(prep, probe);

  ASSERT_EQ(renderer.draws.size(), 1U);
  EXPECT_EQ(renderer.draws.front().mesh, quad);
  EXPECT_EQ(renderer.draws.front().shader, shadow);
  EXPECT_EQ(renderer.draws.front().model, shadow_model(4.0F));
  EXPECT_EQ(renderer.get_current_shader(), previous);
}

TEST(CreatureSubmissionContext, ResolvesTheRendererContextThroughWrappers) {
  ShaderRecordingRenderer renderer;
  Render::GL::PrimitiveBatcher batcher;
  Render::GL::BatchingSubmitter batching(&renderer, &batcher);
  Render::GL::UnseenSubmitter unseen(batching);
  ScenePassProbeSubmitter probe(unseen);

  CreatureSubmissionContext* const context = resolve_creature_submission(probe);
  ASSERT_NE(context, nullptr);
  EXPECT_EQ(context, renderer.creature_submission());
  EXPECT_EQ(&context->rigged_meshes(), &renderer.rigged_mesh_cache());
  EXPECT_EQ(&context->snapshot_meshes(), &renderer.snapshot_mesh_cache());

  Render::GL::Shader* const shader = fake_shader(0x400);
  context->set_current_shader(shader);
  EXPECT_EQ(renderer.get_current_shader(), shader);
  renderer.set_current_shader(nullptr);
  EXPECT_EQ(context->current_shader(), nullptr);
}

TEST(CreatureSubmissionContext, PlainSinksHaveNoContext) {
  StubSubmitter sink;
  ScenePassProbeSubmitter probe(sink);
  EXPECT_EQ(resolve_creature_submission(sink), nullptr);
  EXPECT_EQ(resolve_creature_submission(probe), nullptr);
}

TEST(CreatureSubmissionContext, DetachedRiggedCacheIsStablePerThread) {
  EXPECT_EQ(&detached_rigged_mesh_cache(), &detached_rigged_mesh_cache());
}

TEST(FlushShadowBatch, SinkWithItsOwnContextDrawsWithoutARenderer) {
  Render::GL::Shader* const previous = fake_shader(0x100);
  Render::GL::Shader* const shadow = fake_shader(0x200);
  Render::GL::Mesh* const quad = fake_mesh(0x300);

  HumanoidShadowBatch batch;
  batch.init(shadow, quad, QVector2D(0.0F, 1.0F));
  batch.add(shadow_model(5.0F), 0.25F);
  batch.add(shadow_model(6.0F), 0.5F, RenderPassIntent::Shadow);

  DetachedContextSubmitter sink;
  sink.current_shader = previous;
  ScenePassProbeSubmitter probe(sink);
  flush_shadow_batch(batch, probe);

  ASSERT_EQ(sink.draws.size(), 1U);
  EXPECT_EQ(sink.draws.front().mesh, quad);
  EXPECT_EQ(sink.draws.front().shader, shadow);
  EXPECT_EQ(sink.draws.front().model, shadow_model(5.0F));
  EXPECT_EQ(sink.current_shader, previous);
}

} // namespace
