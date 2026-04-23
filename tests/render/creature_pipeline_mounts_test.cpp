// Phase C — horse / elephant / mounted CreaturePipeline tests.
//
// Exercises process() pose-hook dispatch and submit() LOD/equipment
// dispatch for the Horse / Elephant / Mounted spec kinds via a
// recording ISubmitter.

#include "render/creature/pipeline/creature_frame.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/equipment_registry.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

using namespace Render::Creature::Pipeline;
using Render::Creature::CreatureLOD;

struct RecordedRigged {
  std::uint32_t bone_count{0};
};

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  std::vector<RecordedRigged> rigged_calls;
  std::size_t mesh_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
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

class RecordingRenderer : public Render::GL::Renderer {
public:
  std::vector<Render::GL::RiggedCreatureCmd> rigged_calls;

  void rigged(const Render::GL::RiggedCreatureCmd &cmd) override {
    rigged_calls.push_back(cmd);
  }
};

void horse_pose_hook(const Render::GL::DrawContext &,
                     const Render::GL::HumanoidAnimationContext &,
                     std::uint32_t seed, Render::GL::HumanoidPose &io_pose) {
  io_pose.head_pos.setY(static_cast<float>(seed) + 200.0F);
}

// Helper: a HorseSpecPose with reasonable non-zero half-extents so the
// rigged-mesh walker has something coherent to chew on.
auto make_test_horse_pose() -> Render::Horse::HorseSpecPose {
  Render::GL::HorseDimensions dims{};
  dims.body_length = 1.0F;
  dims.body_width = 0.4F;
  dims.body_height = 0.8F;
  dims.barrel_center_y = 1.0F;
  dims.neck_length = 0.5F;
  dims.neck_rise = 0.3F;
  dims.head_length = 0.4F;
  dims.head_width = 0.25F;
  dims.head_height = 0.3F;
  dims.muzzle_length = 0.18F;
  dims.leg_length = 0.9F;
  dims.hoof_height = 0.06F;
  dims.tail_length = 0.6F;
  dims.saddle_thickness = 0.05F;
  dims.saddle_height = 1.2F;
  dims.idle_bob_amplitude = 0.01F;
  dims.move_bob_amplitude = 0.02F;
  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose(dims, /*bob*/ 0.0F, pose);
  return pose;
}

auto make_test_elephant_pose() -> Render::Elephant::ElephantSpecPose {
  Render::GL::ElephantDimensions dims{};
  dims.body_length = 2.5F;
  dims.body_width = 1.2F;
  dims.body_height = 1.8F;
  dims.barrel_center_y = 2.0F;
  dims.neck_length = 0.8F;
  dims.head_length = 0.7F;
  dims.head_width = 0.5F;
  dims.head_height = 0.6F;
  dims.trunk_length = 1.3F;
  dims.trunk_base_radius = 0.18F;
  dims.trunk_tip_radius = 0.06F;
  dims.leg_length = 1.6F;
  dims.leg_radius = 0.18F;
  dims.foot_radius = 0.20F;
  dims.idle_bob_amplitude = 0.01F;
  dims.move_bob_amplitude = 0.02F;
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose(dims, /*bob*/ 0.0F, pose);
  return pose;
}

TEST(CreaturePipelineHorseProcess, RunsAndStampsLodCountersForHorseRows) {
  CreaturePipeline pipeline;
  FrameContext ctx;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Horse;

  CreatureFrame f;
  Render::Horse::HorseSpecPose pose = make_test_horse_pose();
  Render::GL::HorseVariant variant{};

  f.push_horse(/*id*/ 1, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 42,
               CreatureLOD::Full, pose, variant);
  f.push_horse(/*id*/ 2, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 7,
               CreatureLOD::Reduced, pose, variant);
  f.push_horse(/*id*/ 3, QMatrix4x4{}, /*spec*/ 0, /*seed*/ 9,
               CreatureLOD::Minimal, pose, variant);

  ASSERT_TRUE(frame_columns_consistent(f));

  // process() doesn't currently mutate horse/elephant rows but it must
  // walk them without complaint and bump LOD counters.
  const auto stats = pipeline.process(ctx, specs, f);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
}

TEST(CreaturePipelineHorseSubmit, EmitsOneRiggedCallPerLodRow) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingSubmitter sink;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Horse;

  CreatureFrame f;
  Render::Horse::HorseSpecPose pose = make_test_horse_pose();
  Render::GL::HorseVariant variant{};

  f.push_horse(1, QMatrix4x4{}, 0, 1, CreatureLOD::Full, pose, variant);
  f.push_horse(2, QMatrix4x4{}, 0, 2, CreatureLOD::Reduced, pose, variant);
  f.push_horse(3, QMatrix4x4{}, 0, 3, CreatureLOD::Minimal, pose, variant);
  f.push_horse(4, QMatrix4x4{}, 0, 4, CreatureLOD::Billboard, pose, variant);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 4u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
  EXPECT_EQ(stats.lod_billboard, 1u);
  EXPECT_EQ(stats.equipment_submitted, 0u);

  EXPECT_EQ(sink.rigged_calls.size(), 3u);
}

TEST(CreaturePipelineHorseSubmit, RendererPathUsesGenericHorseAsset) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingRenderer sink;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Horse;

  CreatureFrame f;
  Render::Horse::HorseSpecPose pose = make_test_horse_pose();
  Render::GL::HorseVariant variant{};
  variant.coat_color = QVector3D(0.45F, 0.30F, 0.20F);
  variant.mane_color = QVector3D(0.12F, 0.08F, 0.05F);
  variant.tail_color = QVector3D(0.12F, 0.08F, 0.05F);
  variant.muzzle_color = QVector3D(0.22F, 0.17F, 0.12F);
  variant.hoof_color = QVector3D(0.07F, 0.06F, 0.05F);

  f.push_horse(1, QMatrix4x4{}, 0, 1, CreatureLOD::Full, pose, variant);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_EQ(stats.lod_full, 1u);
  ASSERT_EQ(sink.rigged_calls.size(), 1u);
  EXPECT_GT(sink.rigged_calls[0].bone_count, 0u);
  EXPECT_EQ(sink.rigged_calls[0].role_color_count, 8u);
  EXPECT_EQ(sink.rigged_calls[0].role_colors[0], variant.coat_color);
  EXPECT_EQ(sink.rigged_calls[0].role_colors[3], variant.hoof_color);
}

TEST(CreaturePipelinePreparedSubmit, HorseUsesPreparedPipelineBridge) {
  RecordingSubmitter sink;

  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Horse;
  Render::Horse::HorseSpecPose pose = make_test_horse_pose();
  Render::GL::HorseVariant variant{};
  QMatrix4x4 world;
  world.setToIdentity();

  PreparedCreatureSubmitBatch batch;
  batch.add(make_prepared_horse_row(spec, pose, variant, world, /*seed*/ 77,
                                    CreatureLOD::Full));
  const SubmitStats stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_GT(sink.rigged_calls.size(), 0u);
}

TEST(CreatureRenderStatePrep, HorseRowAppendsResolvedState) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;
  Render::Horse::HorseSpecPose pose = make_test_horse_pose();
  Render::GL::HorseVariant variant{};
  QMatrix4x4 world;
  world.setToIdentity();

  const PreparedCreatureRenderRow row = make_prepared_horse_row(
      spec, pose, variant, world, /*seed*/ 77, CreatureLOD::Minimal);

  EXPECT_EQ(row.spec.kind, CreatureKind::Horse);
  EXPECT_EQ(row.seed, 77u);
  EXPECT_EQ(row.lod, CreatureLOD::Minimal);

  CreatureFrame frame;
  append_prepared_row(frame, row);

  ASSERT_EQ(frame.size(), 1u);
  EXPECT_TRUE(frame_columns_consistent(frame));
  EXPECT_EQ(frame.seed[0], 77u);
  EXPECT_EQ(frame.lod[0], CreatureLOD::Minimal);
  EXPECT_EQ(frame.render_kind[0], CreatureKind::Horse);
  EXPECT_NE(frame.creature_asset_id[0], kInvalidCreatureAsset);
  EXPECT_EQ(frame.role_color_count[0], 8u);
  EXPECT_EQ(frame.role_colors[0][0], variant.coat_color);
  EXPECT_FLOAT_EQ(frame.horse_pose[0].barrel_center.y(),
                  pose.barrel_center.y());
}

TEST(CreaturePipelineElephantSubmit, EmitsOneRiggedCallPerLodRow) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingSubmitter sink;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Elephant;

  CreatureFrame f;
  Render::Elephant::ElephantSpecPose pose = make_test_elephant_pose();
  Render::GL::ElephantVariant variant{};

  f.push_elephant(1, QMatrix4x4{}, 0, 1, CreatureLOD::Full, pose, variant);
  f.push_elephant(2, QMatrix4x4{}, 0, 2, CreatureLOD::Reduced, pose, variant);
  f.push_elephant(3, QMatrix4x4{}, 0, 3, CreatureLOD::Minimal, pose, variant);
  f.push_elephant(4, QMatrix4x4{}, 0, 4, CreatureLOD::Billboard, pose, variant);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 4u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
  EXPECT_EQ(stats.lod_billboard, 1u);
  EXPECT_EQ(sink.rigged_calls.size(), 3u);
}

TEST(CreaturePipelineElephantSubmit, RendererPathUsesGenericElephantAsset) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingRenderer sink;

  std::array<UnitVisualSpec, 1> specs{};
  specs[0].kind = CreatureKind::Elephant;

  CreatureFrame f;
  Render::Elephant::ElephantSpecPose pose = make_test_elephant_pose();
  Render::GL::ElephantVariant variant{};
  variant.skin_color = QVector3D(0.46F, 0.42F, 0.38F);
  variant.ear_inner_color = QVector3D(0.62F, 0.48F, 0.44F);
  variant.tusk_color = QVector3D(0.86F, 0.82F, 0.72F);
  variant.toenail_color = QVector3D(0.28F, 0.24F, 0.20F);

  f.push_elephant(1, QMatrix4x4{}, 0, 1, CreatureLOD::Full, pose, variant);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_EQ(stats.lod_full, 1u);
  ASSERT_EQ(sink.rigged_calls.size(), 1u);
  EXPECT_GT(sink.rigged_calls[0].bone_count, 0u);
  EXPECT_EQ(sink.rigged_calls[0].role_color_count,
            Render::Elephant::kElephantRoleCount);
  EXPECT_EQ(sink.rigged_calls[0].role_colors[0], variant.skin_color);
  EXPECT_EQ(sink.rigged_calls[0].role_colors[5], variant.ear_inner_color);
  EXPECT_EQ(sink.rigged_calls[0].role_colors[6], variant.tusk_color);
}

TEST(CreaturePipelinePreparedSubmit, ElephantUsesPreparedPipelineBridge) {
  RecordingSubmitter sink;

  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Elephant;
  Render::Elephant::ElephantSpecPose pose = make_test_elephant_pose();
  Render::GL::ElephantVariant variant{};
  QMatrix4x4 world;
  world.setToIdentity();

  PreparedCreatureSubmitBatch batch;
  batch.add(make_prepared_elephant_row(spec, pose, variant, world, /*seed*/ 88,
                                       CreatureLOD::Full));
  const SubmitStats stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
  EXPECT_GT(sink.rigged_calls.size(), 0u);
}

TEST(CreatureRenderStatePrep, ElephantRowAppendsResolvedState) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Horse;
  Render::Elephant::ElephantSpecPose pose = make_test_elephant_pose();
  Render::GL::ElephantVariant variant{};
  QMatrix4x4 world;
  world.setToIdentity();

  const PreparedCreatureRenderRow row = make_prepared_elephant_row(
      spec, pose, variant, world, /*seed*/ 88, CreatureLOD::Full);

  EXPECT_EQ(row.spec.kind, CreatureKind::Elephant);
  EXPECT_EQ(row.seed, 88u);
  EXPECT_EQ(row.lod, CreatureLOD::Full);

  CreatureFrame frame;
  append_prepared_row(frame, row);

  ASSERT_EQ(frame.size(), 1u);
  EXPECT_TRUE(frame_columns_consistent(frame));
  EXPECT_EQ(frame.seed[0], 88u);
  EXPECT_EQ(frame.lod[0], CreatureLOD::Full);
  EXPECT_EQ(frame.render_kind[0], CreatureKind::Elephant);
  EXPECT_NE(frame.creature_asset_id[0], kInvalidCreatureAsset);
  EXPECT_EQ(frame.role_color_count[0], Render::Elephant::kElephantRoleCount);
  EXPECT_EQ(frame.role_colors[0][0], variant.skin_color);
  EXPECT_FLOAT_EQ(frame.elephant_pose[0].barrel_center.y(),
                  pose.barrel_center.y());
}

TEST(CreaturePipelinePreparedSubmit, MixedRowsSubmitAsPreparedFrame) {
  RecordingSubmitter sink;

  UnitVisualSpec horse_spec{};
  horse_spec.kind = CreatureKind::Horse;
  UnitVisualSpec elephant_spec{};
  elephant_spec.kind = CreatureKind::Elephant;
  UnitVisualSpec humanoid_spec{};
  humanoid_spec.kind = CreatureKind::Humanoid;

  QMatrix4x4 world;
  world.setToIdentity();

  Render::GL::HumanoidPose humanoid_pose{};
  Render::GL::HumanoidVariant humanoid_variant{};
  Render::GL::HumanoidAnimationContext humanoid_anim{};

  PreparedCreatureSubmitBatch batch;
  batch.reserve(3);
  batch.add(make_prepared_horse_row(horse_spec, make_test_horse_pose(),
                                    Render::GL::HorseVariant{}, world,
                                    /*seed*/ 11, CreatureLOD::Full));
  batch.add(make_prepared_elephant_row(elephant_spec, make_test_elephant_pose(),
                                       Render::GL::ElephantVariant{}, world,
                                       /*seed*/ 22, CreatureLOD::Reduced));
  batch.add(make_prepared_humanoid_row(humanoid_spec, humanoid_pose,
                                       humanoid_variant, humanoid_anim, world,
                                       /*seed*/ 33, CreatureLOD::Billboard));

  const SubmitStats stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 3u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_reduced, 1u);
  EXPECT_EQ(stats.lod_billboard, 1u);
  EXPECT_GT(sink.rigged_calls.size(), 0u);
}

TEST(CreaturePipelineMountedSubmit,
     ComposesMountAndRiderWithRiderAtSocketWorld) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingSubmitter sink;

  std::array<UnitVisualSpec, 2> specs{};
  specs[0].kind = CreatureKind::Horse;
  specs[0].debug_name = "horse";
  specs[1].kind = CreatureKind::Humanoid;
  specs[1].debug_name = "rider";
  specs[1].pose_hook = &horse_pose_hook;

  CreatureFrame f;
  Render::Horse::HorseSpecPose horse_pose = make_test_horse_pose();
  Render::GL::HorseVariant horse_variant{};
  Render::GL::HumanoidPose rider_pose{};
  rider_pose.head_pos = QVector3D(0.0F, 0.0F, 0.0F);
  Render::GL::HumanoidVariant rider_variant{};
  Render::GL::HumanoidAnimationContext rider_anim{};

  QMatrix4x4 mount_world;
  mount_world.translate(10.0F, 0.0F, 5.0F);
  QMatrix4x4 rider_world = mount_world;
  rider_world.translate(0.0F, 1.2F, 0.0F);

  f.push_horse(/*id*/ 1, mount_world, /*spec*/ 0, /*seed*/ 17,
               CreatureLOD::Full, horse_pose, horse_variant);
  f.push_humanoid(/*id*/ 2, rider_world, /*spec*/ 1, /*seed*/ 17,
                  CreatureLOD::Full, rider_pose, rider_variant, rider_anim);

  ASSERT_TRUE(frame_columns_consistent(f));
  EXPECT_EQ(f.render_kind[0], CreatureKind::Horse);
  EXPECT_EQ(f.render_kind[1], CreatureKind::Humanoid);
  EXPECT_EQ(f.role_color_count[0], 8u);
  EXPECT_EQ(f.role_color_count[1], Render::Humanoid::kHumanoidRoleCount);

  pipeline.process(ctx, specs, f);
  EXPECT_FLOAT_EQ(f.humanoid_pose[1].head_pos.y(), 217.0F);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.lod_full, 2u);
  EXPECT_EQ(stats.lod_reduced, 0u);
  EXPECT_EQ(stats.equipment_submitted, 0u);
  EXPECT_EQ(sink.rigged_calls.size(), 2u);
}

TEST(CreaturePipelineMountedSubmit, ElephantHowdahCompositionDispatches) {
  CreaturePipeline pipeline;
  FrameContext ctx;
  RecordingSubmitter sink;

  std::array<UnitVisualSpec, 2> specs{};
  specs[0].kind = CreatureKind::Elephant;
  specs[0].debug_name = "elephant";
  specs[1].kind = CreatureKind::Humanoid;
  specs[1].debug_name = "rider";

  CreatureFrame f;
  Render::Elephant::ElephantSpecPose epose = make_test_elephant_pose();
  Render::GL::ElephantVariant evariant{};
  Render::GL::HumanoidPose rider_pose{};
  Render::GL::HumanoidVariant rider_variant{};
  Render::GL::HumanoidAnimationContext rider_anim{};

  QMatrix4x4 mount_world;
  mount_world.translate(0.0F, 0.0F, 0.0F);
  QMatrix4x4 rider_world = mount_world;
  rider_world.translate(0.0F, 2.5F, 0.0F);

  f.push_elephant(1, mount_world, 0, 33, CreatureLOD::Reduced, epose, evariant);
  f.push_humanoid(2, rider_world, 1, 33, CreatureLOD::Reduced, rider_pose,
                  rider_variant, rider_anim);

  ASSERT_TRUE(frame_columns_consistent(f));
  EXPECT_EQ(f.render_kind[0], CreatureKind::Elephant);
  EXPECT_EQ(f.render_kind[1], CreatureKind::Humanoid);

  const auto stats = pipeline.submit(ctx, specs, f, sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.lod_reduced, 2u);
  EXPECT_EQ(sink.rigged_calls.size(), 2u);
}

} // namespace
