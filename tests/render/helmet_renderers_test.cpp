#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/helmets/carthage_heavy_helmet.h"
#include "render/equipment/helmets/carthage_light_helmet.h"
#include "render/equipment/helmets/headwrap.h"
#include "render/equipment/helmets/roman_heavy_helmet.h"
#include "render/equipment/helmets/roman_light_helmet.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/palette.h"
#include "render/submitter.h"
#include <gtest/gtest.h>
#include <memory>

using namespace Render::GL;

namespace {

using MockSubmitter = EquipmentBatch;

class CountingSubmitter : public ISubmitter {
public:
  void mesh(Mesh *, const QMatrix4x4 &, const QVector3D &, Texture * = nullptr,
            float = 1.0F, int = 0) override {
    ++draw_count;
  }

  void part(Mesh *, Material *, const QMatrix4x4 &, const QVector3D &,
            Texture * = nullptr, float = 1.0F, int = 0) override {
    ++draw_count;
  }

  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float = 1.0F) override {
    ++draw_count;
  }

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

  int draw_count{0};
};

inline int draw_count_of(const EquipmentBatch &b) {
  CountingSubmitter submitter;
  submit_equipment_batch(b, submitter);
  return submitter.draw_count;
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

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(submitter), 12);
}

TEST_F(HelmetRenderersTest, CarthageHeavyHelmetHandlesZeroHeadRadius) {
  CarthageHeavyHelmetRenderer helmet;
  frames.head.radius = 0.0F;

  helmet.render(ctx, frames, palette, anim, submitter);

  // Should not render anything when head radius is zero
  EXPECT_EQ(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, CarthageLightHelmetRendersWithValidFrames) {
  CarthageLightHelmetRenderer helmet;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(submitter), 20);
}

TEST_F(HelmetRenderersTest, CarthageLightHelmetHandlesZeroHeadRadius) {
  CarthageLightHelmetRenderer helmet;
  frames.head.radius = 0.0F;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_EQ(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, RomanHeavyHelmetRendersWithValidFrames) {
  RomanHeavyHelmetRenderer helmet;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(submitter), 7);
}

TEST_F(HelmetRenderersTest, RomanLightHelmetRendersWithValidFrames) {
  RomanLightHelmetRenderer helmet;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(submitter), 8);
}

TEST_F(HelmetRenderersTest, HeadwrapRendersWithValidFrames) {
  HeadwrapRenderer headwrap;

  headwrap.render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GT(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HeadwrapHandlesZeroHeadRadius) {
  HeadwrapRenderer headwrap;
  frames.head.radius = 0.0F;

  headwrap.render(ctx, frames, palette, anim, submitter);

  // Should not render anything when head radius is zero
  EXPECT_EQ(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HelmetsRegisteredInEquipmentRegistry) {
  auto &registry = EquipmentRegistry::instance();

  // Verify Carthage heavy helmet is registered
  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "carthage_heavy"));
  auto carthage_heavy =
      registry.get(EquipmentCategory::Helmet, "carthage_heavy");
  ASSERT_NE(carthage_heavy, nullptr);

  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "carthage_light"));
  auto carthage_light =
      registry.get(EquipmentCategory::Helmet, "carthage_light");
  ASSERT_NE(carthage_light, nullptr);

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

  EXPECT_GT(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, CarthageLightHelmetFromRegistryRenders) {
  auto &registry = EquipmentRegistry::instance();
  auto helmet = registry.get(EquipmentCategory::Helmet, "carthage_light");
  ASSERT_NE(helmet, nullptr);

  helmet->render(ctx, frames, palette, anim, submitter);

  EXPECT_GE(draw_count_of(submitter), 20);
}

TEST_F(HelmetRenderersTest, HeadwrapFromRegistryRenders) {
  auto &registry = EquipmentRegistry::instance();
  auto headwrap = registry.get(EquipmentCategory::Helmet, "headwrap");
  ASSERT_NE(headwrap, nullptr);

  headwrap->render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GT(draw_count_of(submitter), 0);
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

  EXPECT_GE(draw_count_of(submitter1), 12);

  CarthageLightHelmetRenderer light_helmet;
  MockSubmitter submitter_light;
  light_helmet.render(ctx, frames, palette, anim, submitter_light);
  EXPECT_GE(draw_count_of(submitter_light), 20);

  HeadwrapRenderer headwrap;
  MockSubmitter submitter2;
  headwrap.render(ctx, frames, palette, anim, submitter2);

  EXPECT_GT(draw_count_of(submitter2), 0);
}
