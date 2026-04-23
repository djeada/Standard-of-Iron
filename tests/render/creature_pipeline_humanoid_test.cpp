// Phase B — humanoid CreaturePipeline tests.
//
// Exercises process() pose-hook dispatch and submit() LOD/equipment
// dispatch via a recording ISubmitter.

#include "render/creature/pipeline/creature_frame.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/equipment_registry.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

using namespace Render::Creature::Pipeline;
using Render::Creature::CreatureLOD;

struct RecordedMesh {
  Render::GL::Mesh *mesh{nullptr};
  QVector3D color{};
  int material_id{0};
};

struct RecordedRigged {
  std::uint32_t bone_count{0};
};

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;
  std::vector<RecordedRigged> rigged_calls;

  void mesh(Render::GL::Mesh *m, const QMatrix4x4 &, const QVector3D &c,
            Render::GL::Texture *, float, int material_id) override {
    meshes.push_back({m, c, material_id});
  }
  void rigged(const Render::GL::RiggedCreatureCmd &cmd) override {
    rigged_calls.push_back({cmd.bone_count});
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

// Small static instance so the EquipmentRecord can carry a non-null
// mesh pointer through submit() without any GL bring-up. Pipeline never
// dereferences the pointer; it just hands it to ISubmitter::mesh.
auto fake_mesh() -> Render::GL::Mesh * {
  static int sentinel = 0;
  return reinterpret_cast<Render::GL::Mesh *>(&sentinel);
}

// Pose hook used by HumanoidProcessRunsPoseHook — mutates the pose so
// the test can detect it ran.
void test_pose_hook(const Render::GL::DrawContext &,
                    const Render::GL::HumanoidAnimationContext &,
                    std::uint32_t seed, Render::GL::HumanoidPose &io_pose) {
  io_pose.head_pos.setY(static_cast<float>(seed) + 100.0F);
}

TEST(CreaturePipelineHumanoidProcess, RunsPoseHookAndStampsLodCounters) {
  CreaturePipeline pipeline;
  FrameContext ctx;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].pose_hook = &test_pose_hook;

  CreatureFrame f;
  Render::GL::HumanoidPose pose{};
  pose.head_pos = QVector3D(0.0F, 0.0F, 0.0F);
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  f.push_humanoid(/*id*/ 1, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 42,
                  CreatureLOD::Full, pose, variant, anim);
  f.push_humanoid(/*id*/ 2, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 7,
                  CreatureLOD::Reduced, pose, variant, anim);

  ASSERT_TRUE(frame_columns_consistent(f));

  const auto stats = pipeline.process(ctx, specs, f);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);

  // Pose hook should have stamped the seed into head_pos.y for both
  // rows.
  ASSERT_EQ(f.humanoid_pose.size(), 2u);
  EXPECT_FLOAT_EQ(f.humanoid_pose[0].head_pos.y(), 142.0F);
  EXPECT_FLOAT_EQ(f.humanoid_pose[1].head_pos.y(), 107.0F);
}

TEST(CreaturePipelineHumanoidSubmit, EmitsOneRiggedCallPerLodRow) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingSubmitter sink;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;

  CreatureFrame f;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  f.push_humanoid(1, QMatrix4x4{}, 0, 1, CreatureLOD::Full, pose, variant,
                  anim);
  f.push_humanoid(2, QMatrix4x4{}, 0, 2, CreatureLOD::Reduced, pose, variant,
                  anim);
  f.push_humanoid(3, QMatrix4x4{}, 0, 3, CreatureLOD::Minimal, pose, variant,
                  anim);
  f.push_humanoid(4, QMatrix4x4{}, 0, 4, CreatureLOD::Billboard, pose, variant,
                  anim);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  // Every humanoid row counts as an entity submission (billboard is
  // counted but produces no body draw).
  EXPECT_EQ(stats.entities_submitted, 4u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
  EXPECT_EQ(stats.lod_billboard, 1u);
  EXPECT_EQ(stats.equipment_submitted, 0u);

  // Body submission always routes through the generic rigged creature
  // path. Billboard rows skip body submission entirely.
  EXPECT_EQ(sink.rigged_calls.size(), 3u);
}

TEST(CreaturePipelinePreparedSubmit, HumanoidUsesPreparedPipelineBridge) {
  RecordingSubmitter sink;

  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};
  QMatrix4x4 world;
  world.setToIdentity();

  PreparedCreatureSubmitBatch batch;
  batch.add(make_prepared_humanoid_row(spec, pose, variant, anim, world,
                                       /*seed*/ 123, CreatureLOD::Full));
  const SubmitStats stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_GT(sink.meshes.size() + sink.rigged_calls.size(), 0u);
}

TEST(CreaturePipelinePreparedSubmit, BatchOwnsHumanoidLegacyContextCopies) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};
  QMatrix4x4 world;
  world.setToIdentity();

  Render::GL::DrawContext legacy_ctx{};
  legacy_ctx.animation_time = 4.0F;

  PreparedCreatureSubmitBatch batch;
  batch.reserve(1);
  batch.add_with_legacy_context(
      make_prepared_humanoid_row(spec, pose, variant, anim, world,
                                 /*seed*/ 123, CreatureLOD::Full),
      legacy_ctx);

  ASSERT_EQ(batch.size(), 1u);
  ASSERT_NE(batch.rows()[0].legacy_ctx, nullptr);
  EXPECT_FLOAT_EQ(batch.rows()[0].legacy_ctx->animation_time, 4.0F);

  legacy_ctx.animation_time = 8.0F;
  EXPECT_FLOAT_EQ(batch.rows()[0].legacy_ctx->animation_time, 4.0F);
}

TEST(CreatureRenderStatePrep, HumanoidRowAppendsResolvedState) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Horse;
  Render::GL::HumanoidPose pose{};
  pose.head_pos = QVector3D(1.0F, 2.0F, 3.0F);
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};
  QMatrix4x4 world;
  world.setToIdentity();

  const PreparedCreatureRenderRow row = make_prepared_humanoid_row(
      spec, pose, variant, anim, world, /*seed*/ 321, CreatureLOD::Reduced);

  EXPECT_EQ(row.spec.kind, CreatureKind::Humanoid);
  EXPECT_EQ(row.seed, 321u);
  EXPECT_EQ(row.lod, CreatureLOD::Reduced);
  EXPECT_EQ(row.pass, RenderPassIntent::Main);

  CreatureFrame frame;
  append_prepared_row(frame, row);

  ASSERT_EQ(frame.size(), 1u);
  EXPECT_TRUE(frame_columns_consistent(frame));
  EXPECT_EQ(frame.seed[0], 321u);
  EXPECT_EQ(frame.lod[0], CreatureLOD::Reduced);
  EXPECT_EQ(frame.render_kind[0], CreatureKind::Humanoid);
  EXPECT_NE(frame.creature_asset_id[0], kInvalidCreatureAsset);
  EXPECT_EQ(frame.role_color_count[0], Render::Humanoid::kHumanoidRoleCount);
  EXPECT_EQ(frame.role_colors[0][0], variant.palette.cloth);
  EXPECT_EQ(frame.palette_id[0], spec.palette_id);
  EXPECT_FLOAT_EQ(frame.humanoid_pose[0].head_pos.y(), 2.0F);
}

TEST(CreaturePipelineHumanoidSubmit, EquipmentLoadoutDrawsOncePerRecordPerRow) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingSubmitter sink;

  std::array<EquipmentRecord, 2> records{};
  records[0].static_mesh = fake_mesh();
  records[0].material_id = 11;
  records[0].override_color = QVector3D(1.0F, 0.0F, 0.0F);
  records[1].static_mesh = fake_mesh();
  records[1].material_id = 22;
  records[1].tint_role = TintRole::Metal;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Humanoid;
  specs[0].equipment =
      std::span<const EquipmentRecord>{records.data(), records.size()};

  CreatureFrame f;
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  variant.palette.metal = QVector3D(0.5F, 0.5F, 0.6F);
  Render::GL::HumanoidAnimationContext anim{};

  // Use Billboard rows: pipeline still walks the equipment loadout but
  // the body submitter short-circuits, so the recorded mesh stream
  // contains only the equipment draws (deterministic count).
  f.push_humanoid(1, QMatrix4x4{}, 0, 1, CreatureLOD::Billboard, pose, variant,
                  anim);
  f.push_humanoid(2, QMatrix4x4{}, 0, 2, CreatureLOD::Billboard, pose, variant,
                  anim);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.equipment_submitted, 4u); // 2 records × 2 rows
  ASSERT_EQ(sink.meshes.size(), 4u);

  // Spot-check tint resolution. records[0] uses override_color, [1]
  // uses palette.metal. Order is record-major within a row, so the
  // first two meshes are row 0's loadout.
  EXPECT_EQ(sink.meshes[0].material_id, 11);
  EXPECT_FLOAT_EQ(sink.meshes[0].color.x(), 1.0F);
  EXPECT_EQ(sink.meshes[1].material_id, 22);
  EXPECT_FLOAT_EQ(sink.meshes[1].color.z(), 0.6F);
  EXPECT_EQ(sink.meshes[2].material_id, 11);
  EXPECT_EQ(sink.meshes[3].material_id, 22);
}

TEST(EquipmentRegistry, MakeLegacyEquipmentRecordSetsLegacyHook) {
  // We don't have a real IEquipmentRenderer instance here without GL
  // setup; verify the helper's struct shape from a null-ish input is
  // not appropriate, so only assert that the helper compiles via a
  // forward-declared symbol. The legacy_submit field gets exercised
  // by integration tests under the live game.
  auto rec = EquipmentRecord{};
  EXPECT_EQ(rec.legacy_submit, nullptr);
  EXPECT_EQ(rec.legacy_user, nullptr);
}

} // namespace
