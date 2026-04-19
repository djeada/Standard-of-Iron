#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/helmets/carthage_heavy_helmet.h"
#include "render/equipment/helmets/headwrap.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/palette.h"
#include <gtest/gtest.h>
#include <memory>

using namespace Render::GL;

namespace {

using MockSubmitter = EquipmentBatch;

inline int mesh_count_of(const EquipmentBatch &b) {
  return static_cast<int>(b.meshes.size());
}

} // namespace

namespace {

// Helper to create a basic DrawContext
DrawContext createTestContext() {
  DrawContext ctx;
  ctx.model.setToIdentity();
  ctx.backend = nullptr;
  ctx.entity = nullptr;
  return ctx;
}

// Helper to create basic body frames
BodyFrames createTestFrames() {
  using HP = HumanProportions;
  BodyFrames frames;
  float const head_center_y = HP::HEAD_CENTER_Y;
  frames.head.origin = QVector3D(0.0F, head_center_y, 0.0F);
  frames.head.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.head.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.head.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.head.radius = HP::HEAD_RADIUS * 1.05F;
  return frames;
}

// Helper to create basic palette
HumanoidPalette createTestPalette() {
  HumanoidPalette palette;
  palette.skin = QVector3D(0.8F, 0.6F, 0.5F);
  palette.cloth = QVector3D(0.7F, 0.3F, 0.2F);
  palette.leather = QVector3D(0.4F, 0.3F, 0.2F);
  palette.leather_dark = QVector3D(0.3F, 0.2F, 0.1F);
  palette.metal = QVector3D(0.7F, 0.7F, 0.7F);
  palette.wood = QVector3D(0.5F, 0.3F, 0.2F);
  return palette;
}

} // namespace

class HelmetRenderersTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Ensure built-in equipment is registered
    register_built_in_equipment();

    ctx = createTestContext();
    frames = createTestFrames();
    palette = createTestPalette();
    anim.inputs.time = 0.0F;
    anim.inputs.is_moving = false;
    anim.inputs.is_attacking = false;
    anim.inputs.is_melee = false;
  }

  DrawContext ctx;
  BodyFrames frames;
  HumanoidPalette palette;
  HumanoidAnimationContext anim;
  MockSubmitter submitter;
};

TEST_F(HelmetRenderersTest, CarthageHeavyHelmetRendersWithValidFrames) {
  CarthageHeavyHelmetRenderer helmet;

  helmet.render(ctx, frames, palette, anim, submitter);

  // Carthage heavy helmet should render multiple mesh components
  EXPECT_GT(mesh_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, CarthageHeavyHelmetHandlesZeroHeadRadius) {
  CarthageHeavyHelmetRenderer helmet;
  frames.head.radius = 0.0F;

  helmet.render(ctx, frames, palette, anim, submitter);

  // Should not render anything when head radius is zero
  EXPECT_EQ(mesh_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HeadwrapRendersWithValidFrames) {
  HeadwrapRenderer headwrap;

  headwrap.render(ctx, frames, palette, anim, submitter);

  // Headwrap should render band, knot, and tail
  EXPECT_GT(mesh_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HeadwrapHandlesZeroHeadRadius) {
  HeadwrapRenderer headwrap;
  frames.head.radius = 0.0F;

  headwrap.render(ctx, frames, palette, anim, submitter);

  // Should not render anything when head radius is zero
  EXPECT_EQ(mesh_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HelmetsRegisteredInEquipmentRegistry) {
  auto &registry = EquipmentRegistry::instance();

  // Verify Carthage heavy helmet is registered
  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "carthage_heavy"));
  auto carthage_heavy =
      registry.get(EquipmentCategory::Helmet, "carthage_heavy");
  ASSERT_NE(carthage_heavy, nullptr);

  // Verify headwrap is registered
  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "headwrap"));
  auto headwrap = registry.get(EquipmentCategory::Helmet, "headwrap");
  ASSERT_NE(headwrap, nullptr);
}

TEST_F(HelmetRenderersTest, CarthageHeavyHelmetFromRegistryRenders) {
  auto &registry = EquipmentRegistry::instance();
  auto helmet = registry.get(EquipmentCategory::Helmet, "carthage_heavy");
  ASSERT_NE(helmet, nullptr);

  helmet->render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(mesh_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HeadwrapFromRegistryRenders) {
  auto &registry = EquipmentRegistry::instance();
  auto headwrap = registry.get(EquipmentCategory::Helmet, "headwrap");
  ASSERT_NE(headwrap, nullptr);

  headwrap->render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(mesh_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HelmetsUseHeadFrameCoordinates) {
  // Test that helmets use head frame's coordinate system
  frames.head.origin = QVector3D(1.0F, 2.0F, 3.0F);
  frames.head.right = QVector3D(0.0F, 1.0F, 0.0F); // Rotated frame
  frames.head.up = QVector3D(-1.0F, 0.0F, 0.0F);
  frames.head.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.head.radius = 0.12F;

  CarthageHeavyHelmetRenderer helmet;
  MockSubmitter submitter1;
  helmet.render(ctx, frames, palette, anim, submitter1);

  // Helmet should still render even with rotated frame
  EXPECT_GT(mesh_count_of(submitter1), 0);

  HeadwrapRenderer headwrap;
  MockSubmitter submitter2;
  headwrap.render(ctx, frames, palette, anim, submitter2);

  EXPECT_GT(mesh_count_of(submitter2), 0);
}
