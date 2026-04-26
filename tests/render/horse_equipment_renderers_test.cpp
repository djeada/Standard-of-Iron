#include "render/entity/registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/horse/armor/champion_renderer.h"
#include "render/equipment/horse/armor/crupper_renderer.h"
#include "render/equipment/horse/armor/leather_barding_renderer.h"
#include "render/equipment/horse/armor/scale_barding_renderer.h"
#include "render/equipment/horse/decorations/saddle_bag_renderer.h"
#include "render/equipment/horse/i_horse_equipment_renderer.h"
#include "render/equipment/horse/tack/blanket_renderer.h"
#include "render/equipment/horse/tack/bridle_renderer.h"
#include "render/equipment/horse/tack/reins_renderer.h"
#include "render/gl/primitives.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace Render::GL;

namespace {

using MockSubmitter = EquipmentBatch;
using CapturingSubmitter = EquipmentBatch;

inline int mesh_count_of(const EquipmentBatch &b) {
  return static_cast<int>(b.meshes.size());
}
inline int cylinder_count_of(const EquipmentBatch &b) {
  return static_cast<int>(b.cylinders.size());
}
inline int archetype_count_of(const EquipmentBatch &b) {
  return static_cast<int>(b.archetypes.size());
}
inline float axis_scale_of(const QMatrix4x4 &m, int column) {
  return m.column(column).toVector3D().length();
}

} // namespace

class HorseEquipmentRenderersTest : public ::testing::Test {
protected:
  void SetUp() override {
    ctx.model.setToIdentity();
    ctx.entity = nullptr;

    frames.back_center.origin = QVector3D(0.0F, 1.0F, 0.0F);
    frames.back_center.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.back_center.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.back_center.forward = QVector3D(0.0F, 0.0F, 1.0F);

    frames.head.origin = QVector3D(0.0F, 1.5F, 1.0F);
    frames.head.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.head.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.head.forward = QVector3D(0.0F, 0.0F, 1.0F);

    frames.muzzle.origin = QVector3D(0.0F, 1.4F, 1.2F);
    frames.muzzle.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.muzzle.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.muzzle.forward = QVector3D(0.0F, 0.0F, 1.0F);

    frames.chest.origin = QVector3D(0.0F, 0.9F, 0.5F);
    frames.chest.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.chest.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.chest.forward = QVector3D(0.0F, 0.0F, 1.0F);

    frames.barrel.origin = QVector3D(0.0F, 0.8F, 0.0F);
    frames.barrel.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.barrel.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.barrel.forward = QVector3D(0.0F, 0.0F, 1.0F);

    frames.rump.origin = QVector3D(0.0F, 0.9F, -0.5F);
    frames.rump.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.rump.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.rump.forward = QVector3D(0.0F, 0.0F, 1.0F);

    frames.tail_base.origin = QVector3D(0.0F, 1.0F, -0.8F);
    frames.tail_base.right = QVector3D(1.0F, 0.0F, 0.0F);
    frames.tail_base.up = QVector3D(0.0F, 1.0F, 0.0F);
    frames.tail_base.forward = QVector3D(0.0F, 0.0F, 1.0F);

    variant.saddle_color = QVector3D(0.6F, 0.4F, 0.2F);
    variant.blanket_color = QVector3D(0.8F, 0.1F, 0.1F);
    variant.tack_color = QVector3D(0.3F, 0.2F, 0.1F);

    anim.time = 0.0F;
    anim.phase = 0.0F;
    anim.bob = 0.0F;
    anim.is_moving = false;
    anim.rider_intensity = 0.0F;
  }

  DrawContext ctx;
  HorseBodyFrames frames;
  HorseVariant variant;
  HorseAnimationContext anim;
};

TEST_F(HorseEquipmentRenderersTest, BridleRendererUsesArchetypePath) {
  BridleRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(cylinder_count_of(batch), 0);
  EXPECT_EQ(mesh_count_of(batch), 0);
  ASSERT_EQ(archetype_count_of(batch), 5);

  MockSubmitter submitter;
  BatchSubmitterAdapter adapter(submitter);
  submit_equipment_batch(batch, adapter);

  EXPECT_EQ(cylinder_count_of(submitter), 0);
  EXPECT_EQ(mesh_count_of(submitter), 5);
}

TEST_F(HorseEquipmentRenderersTest, BlanketRendererProducesMeshes) {
  BlanketRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(mesh_count_of(batch), 0);
  ASSERT_EQ(archetype_count_of(batch), 1);

  MockSubmitter submitter;
  BatchSubmitterAdapter adapter(submitter);
  submit_equipment_batch(batch, adapter);

  ASSERT_GE(mesh_count_of(submitter), 3);
  EXPECT_EQ(submitter.meshes.front().mesh, get_unit_cube());
  EXPECT_LT(axis_scale_of(submitter.meshes.front().model, 0), 0.20F);
  EXPECT_LT(axis_scale_of(submitter.meshes.front().model, 2), 0.35F);
}

TEST_F(HorseEquipmentRenderersTest, ReinsRendererUsesArchetypePath) {
  ReinsRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(cylinder_count_of(batch), 0);
  EXPECT_EQ(mesh_count_of(batch), 0);
  ASSERT_EQ(archetype_count_of(batch), 6);

  MockSubmitter submitter;
  BatchSubmitterAdapter adapter(submitter);
  submit_equipment_batch(batch, adapter);

  EXPECT_EQ(cylinder_count_of(submitter), 0);
  EXPECT_EQ(mesh_count_of(submitter), 6);
}

TEST_F(HorseEquipmentRenderersTest, ReinsRendererRespectsModelTransform) {
  EquipmentBatch batch;

  ctx.model.translate(2.0F, 1.0F, -3.0F);

  ReinsRenderer renderer;
  renderer.render(ctx, frames, variant, anim, batch);

  ASSERT_FALSE(batch.archetypes.empty());

  const HorseAttachmentFrame &muzzle = frames.muzzle;
  QVector3D const expected_local =
      muzzle.origin + muzzle.right * 0.10F + muzzle.forward * 0.10F;
  QVector3D const expected_world = ctx.model.map(expected_local);

  QVector3D const actual =
      batch.archetypes.front().world.column(3).toVector3D();
  EXPECT_NEAR(actual.x(), expected_world.x(), 1e-4F);
  EXPECT_NEAR(actual.y(), expected_world.y(), 1e-4F);
  EXPECT_NEAR(actual.z(), expected_world.z(), 1e-4F);
}

TEST_F(HorseEquipmentRenderersTest, ReinsRendererAddsCrossConnections) {
  EquipmentBatch batch;
  ReinsRenderer renderer;

  renderer.render(ctx, frames, variant, anim, batch);

  ASSERT_GE(static_cast<int>(batch.archetypes.size()), 6);

  auto const connectors = std::count_if(
      batch.archetypes.begin(), batch.archetypes.end(), [](const auto &a) {
        QVector3D const start = a.world.column(3).toVector3D();
        QVector3D const end = start + a.world.column(1).toVector3D();
        return start.x() * end.x() < 0.0F;
      });
  EXPECT_GE(connectors, 2);

  ASSERT_FALSE(batch.archetypes.empty());
  EXPECT_GT(axis_scale_of(batch.archetypes.front().world, 1), 0.5F);
}

TEST_F(HorseEquipmentRenderersTest, ScaleBardingRendererProducesMeshes) {
  ScaleBardingRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(mesh_count_of(batch), 0);
  EXPECT_EQ(archetype_count_of(batch), 3);
}

TEST_F(HorseEquipmentRenderersTest, LeatherBardingRendererProducesMeshes) {
  LeatherBardingRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(mesh_count_of(batch), 0);
  EXPECT_EQ(archetype_count_of(batch), 2);
}

TEST_F(HorseEquipmentRenderersTest, ChampionRendererProducesMeshes) {
  ChampionRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(mesh_count_of(batch), 0);
  EXPECT_EQ(archetype_count_of(batch), 1);
}

TEST_F(HorseEquipmentRenderersTest, CrupperRendererProducesMeshes) {
  CrupperRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(mesh_count_of(batch), 0);
  EXPECT_EQ(archetype_count_of(batch), 1);
}

TEST_F(HorseEquipmentRenderersTest, SaddleBagRendererUsesArchetypePath) {
  SaddleBagRenderer renderer;
  EquipmentBatch batch;

  renderer.render(ctx, frames, variant, anim, batch);

  EXPECT_EQ(cylinder_count_of(batch), 0);
  EXPECT_EQ(mesh_count_of(batch), 0);
  ASSERT_EQ(archetype_count_of(batch), 1);

  MockSubmitter submitter;
  BatchSubmitterAdapter adapter(submitter);
  submit_equipment_batch(batch, adapter);

  EXPECT_EQ(cylinder_count_of(submitter), 0);
  EXPECT_EQ(mesh_count_of(submitter), 4);
}
