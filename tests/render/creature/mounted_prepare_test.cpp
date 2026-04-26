// Phase A regression — mounted prepare module.
//
// Exercises render/entity/mounted_prepare.{h,cpp}: prepare_mounted_rows
// must produce a horse mount row + humanoid rider row carrying the supplied
// pass intent.

#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/mounted_knight_renderer_base.h"
#include "render/entity/mounted_prepare.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/prepare.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>

namespace {

class NullSubmitter : public Render::GL::ISubmitter {
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

TEST(MountedPrepare, ProducesHorseMountAndHumanoidRiderRows) {
  using namespace Render::Creature::Pipeline;

  MountedSpec mounted{};
  mounted.mount.kind = CreatureKind::Horse;
  mounted.mount.debug_name = "test/horse";
  mounted.rider.kind = CreatureKind::Humanoid;
  mounted.rider.debug_name = "test/rider";

  Render::Horse::HorseSpecPose mount_pose{};
  Render::GL::HorseVariant mount_variant{};
  Render::GL::HumanoidPose rider_pose{};
  Render::GL::HumanoidVariant rider_variant{};
  Render::GL::HumanoidAnimationContext rider_anim{};
  QMatrix4x4 mount_world;
  QMatrix4x4 rider_world;

  auto set = Render::GL::prepare_mounted_rows(
      mounted, mount_world, rider_world, mount_pose, mount_variant, rider_pose,
      rider_variant, rider_anim, /*seed*/ 99,
      Render::Creature::CreatureLOD::Full, RenderPassIntent::Shadow);

  EXPECT_EQ(set.mount_row.spec.kind, CreatureKind::Horse);
  EXPECT_EQ(set.rider_row.spec.kind, CreatureKind::Humanoid);
  EXPECT_EQ(set.mount_row.pass, RenderPassIntent::Shadow);
  EXPECT_EQ(set.rider_row.pass, RenderPassIntent::Shadow);
  EXPECT_EQ(set.mount_row.seed, 99u);
  EXPECT_EQ(set.rider_row.seed, 99u);
}

TEST(MountedPrepare, ShadowPairProducesNoDrawCalls) {
  using namespace Render::Creature::Pipeline;

  MountedSpec mounted{};
  mounted.mount.kind = CreatureKind::Horse;
  mounted.rider.kind = CreatureKind::Humanoid;

  Render::Horse::HorseSpecPose mount_pose{};
  Render::GL::HorseVariant mount_variant{};
  Render::GL::HumanoidPose rider_pose{};
  Render::GL::HumanoidVariant rider_variant{};
  Render::GL::HumanoidAnimationContext rider_anim{};
  QMatrix4x4 mount_world;
  QMatrix4x4 rider_world;

  auto set = Render::GL::prepare_mounted_rows(
      mounted, mount_world, rider_world, mount_pose, mount_variant, rider_pose,
      rider_variant, rider_anim, /*seed*/ 1,
      Render::Creature::CreatureLOD::Full, RenderPassIntent::Shadow);

  NullSubmitter sink;
  PreparedCreatureSubmitBatch batch;
  batch.add(set.mount_row);
  batch.add(set.rider_row);
  (void)batch.submit(sink);

  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(MountedPrepare, MainPairProducesTwoEntitySubmissions) {
  using namespace Render::Creature::Pipeline;

  MountedSpec mounted{};
  mounted.mount.kind = CreatureKind::Horse;
  mounted.rider.kind = CreatureKind::Humanoid;

  Render::Horse::HorseSpecPose mount_pose{};
  Render::GL::HorseVariant mount_variant{};
  Render::GL::HumanoidPose rider_pose{};
  Render::GL::HumanoidVariant rider_variant{};
  Render::GL::HumanoidAnimationContext rider_anim{};
  QMatrix4x4 mount_world;
  QMatrix4x4 rider_world;

  auto set = Render::GL::prepare_mounted_rows(
      mounted, mount_world, rider_world, mount_pose, mount_variant, rider_pose,
      rider_variant, rider_anim, /*seed*/ 1,
      Render::Creature::CreatureLOD::Full, RenderPassIntent::Main);

  NullSubmitter sink;
  PreparedCreatureSubmitBatch batch;
  batch.add(set.mount_row);
  batch.add(set.rider_row);
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
}

TEST(MountedPrepare, MountedHumanoidPreparationQueuesRiderAndHorseBodies) {
  using namespace Render::Creature::Pipeline;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  ASSERT_EQ(prep.bodies.size(), 2u);
  int rider_rows = 0;
  int horse_rows = 0;
  float rider_world_y = 0.0F;
  float horse_world_y = 0.0F;
  for (const auto &row : prep.bodies.rows()) {
    if (row.spec.kind == CreatureKind::Humanoid) {
      ++rider_rows;
      rider_world_y = row.world_from_unit.column(3).y();
    }
    if (row.spec.kind == CreatureKind::Horse) {
      ++horse_rows;
      horse_world_y = row.world_from_unit.column(3).y();
    }
  }

  EXPECT_EQ(rider_rows, 1);
  EXPECT_EQ(horse_rows, 1);
  EXPECT_NEAR(rider_world_y, horse_world_y, 0.0001F);
  EXPECT_EQ(prep.bodies.requests().size(), 2u);
  EXPECT_TRUE(owns_slot(renderer.visual_spec().owned_legacy_slots,
                        LegacySlotMask::Attachments));
}

TEST(MountedPrepare, SubmitPreparationDrawsRiderFromPreparedPose) {
  using namespace Render::Creature::Pipeline;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_GT(sink.rigged_calls, 0);
}

} // namespace
