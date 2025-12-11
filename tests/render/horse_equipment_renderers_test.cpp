#include "render/entity/registry.h"
#include "render/equipment/horse/armor/champion_renderer.h"
#include "render/equipment/horse/armor/crupper_renderer.h"
#include "render/equipment/horse/armor/leather_barding_renderer.h"
#include "render/equipment/horse/armor/scale_barding_renderer.h"
#include "render/equipment/horse/decorations/plume_renderer.h"
#include "render/equipment/horse/decorations/saddle_bag_renderer.h"
#include "render/equipment/horse/decorations/tail_ribbon_renderer.h"
#include "render/equipment/horse/i_horse_equipment_renderer.h"
#include "render/equipment/horse/saddles/carthage_saddle_renderer.h"
#include "render/equipment/horse/saddles/light_cavalry_saddle_renderer.h"
#include "render/equipment/horse/saddles/roman_saddle_renderer.h"
#include "render/equipment/horse/tack/blanket_renderer.h"
#include "render/equipment/horse/tack/bridle_renderer.h"
#include "render/equipment/horse/tack/reins_renderer.h"
#include "render/equipment/horse/tack/stirrup_renderer.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace Render::GL;

namespace {

class MockSubmitter : public ISubmitter {
public:
  void mesh(Mesh * /*mesh*/, const QMatrix4x4 & /*model*/,
            const QVector3D & /*color*/, Texture * /*tex*/ = nullptr,
            float /*alpha*/ = 1.0F, int /*materialId*/ = 0) override {
    mesh_count++;
  }

  void cylinder(const QVector3D & /*start*/, const QVector3D & /*end*/,
                float /*radius*/, const QVector3D & /*color*/,
                float /*alpha*/ = 1.0F) override {
    cylinder_count++;
  }

  void selection_ring(const QMatrix4x4 & /*model*/, float /*alphaInner*/,
                      float /*alphaOuter*/,
                      const QVector3D & /*color*/) override {}

  void grid(const QMatrix4x4 & /*model*/, const QVector3D & /*color*/,
            float /*cellSize*/, float /*thickness*/,
            float /*extent*/) override {}

  void selection_smoke(const QMatrix4x4 & /*model*/,
                       const QVector3D & /*color*/,
                       float /*baseAlpha*/ = 0.15F) override {}

  int mesh_count = 0;
  int cylinder_count = 0;
};

class CapturingSubmitter : public MockSubmitter {
public:
  struct CylinderCall {
    QVector3D start;
    QVector3D end;
    float radius;
  };

  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0F) override {
    cylinders.push_back({start, end, radius});
    MockSubmitter::cylinder(start, end, radius, color, alpha);
  }

  std::vector<CylinderCall> cylinders;
};

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

TEST_F(HorseEquipmentRenderersTest, RomanSaddleRendererProducesMeshes) {
  RomanSaddleRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, CarthageSaddleRendererProducesMeshes) {
  CarthageSaddleRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, LightCavalrySaddleRendererProducesMeshes) {
  LightCavalrySaddleRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, BridleRendererProducesCylinders) {
  BridleRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.cylinder_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, StirrupRendererProducesBoth) {
  StirrupRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.cylinder_count, 0);
  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, BlanketRendererProducesMeshes) {
  BlanketRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, ReinsRendererProducesCylinders) {
  ReinsRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.cylinder_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, ReinsRendererRespectsModelTransform) {
  CapturingSubmitter submitter;

  ctx.model.translate(2.0F, 1.0F, -3.0F);

  ReinsRenderer renderer;
  renderer.render(ctx, frames, variant, anim, submitter);

  ASSERT_FALSE(submitter.cylinders.empty());

  const HorseAttachmentFrame &muzzle = frames.muzzle;
  QVector3D const expected_local =
      muzzle.origin + muzzle.right * 0.10F + muzzle.forward * 0.10F;
  QVector3D const expected_world = ctx.model.map(expected_local);

  QVector3D const actual = submitter.cylinders.front().start;
  EXPECT_NEAR(actual.x(), expected_world.x(), 1e-4F);
  EXPECT_NEAR(actual.y(), expected_world.y(), 1e-4F);
  EXPECT_NEAR(actual.z(), expected_world.z(), 1e-4F);
}

TEST_F(HorseEquipmentRenderersTest, ReinsRendererAddsCrossConnections) {
  CapturingSubmitter submitter;
  ReinsRenderer renderer;

  renderer.render(ctx, frames, variant, anim, submitter);

  ASSERT_GE(static_cast<int>(submitter.cylinders.size()), 6);

  auto const connectors = std::count_if(
      submitter.cylinders.begin(), submitter.cylinders.end(),
      [](const auto &c) { return c.start.x() * c.end.x() < 0.0F; });
  EXPECT_GE(connectors, 2);

  ASSERT_FALSE(submitter.cylinders.empty());
  EXPECT_NEAR(submitter.cylinders.front().radius, 0.004F, 1e-4F);
}

TEST_F(HorseEquipmentRenderersTest, ScaleBardingRendererProducesMeshes) {
  ScaleBardingRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, LeatherBardingRendererProducesMeshes) {
  LeatherBardingRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, ChampionRendererProducesMeshes) {
  ChampionRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, CrupperRendererProducesMeshes) {
  CrupperRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, PlumeRendererProducesCylinders) {
  PlumeRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.cylinder_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, TailRibbonRendererProducesBoth) {
  TailRibbonRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.cylinder_count, 0);
  EXPECT_GT(submitter.mesh_count, 0);
}

TEST_F(HorseEquipmentRenderersTest, SaddleBagRendererProducesBoth) {
  SaddleBagRenderer renderer;
  MockSubmitter submitter;

  renderer.render(ctx, frames, variant, anim, submitter);

  EXPECT_GT(submitter.cylinder_count, 0);
  EXPECT_GT(submitter.mesh_count, 0);
}
