#include <gtest/gtest.h>
#include <memory>

#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/helmets/carthage_heavy_helmet.h"
#include "render/equipment/helmets/carthage_light_helmet.h"
#include "render/equipment/helmets/headwrap.h"
#include "render/equipment/helmets/helmet_alignment.h"
#include "render/equipment/helmets/roman_heavy_helmet.h"
#include "render/equipment/helmets/roman_light_helmet.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/palette.h"
#include "render/submitter.h"

using namespace Render::GL;

namespace {

using MockSubmitter = EquipmentBatch;

class CountingSubmitter : public ISubmitter {
public:
  void mesh(Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Texture* = nullptr,
            float = 1.0F,
            int = 0) override {
    ++draw_count;
  }

  void part(Mesh*,
            Material*,
            const QMatrix4x4&,
            const QVector3D&,
            Texture* = nullptr,
            float = 1.0F,
            int = 0) override {
    ++draw_count;
  }

  void cylinder(const QVector3D&,
                const QVector3D&,
                float,
                const QVector3D&,
                float = 1.0F) override {
    ++draw_count;
  }

  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
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

  int draw_count{0};
};

inline int draw_count_of(const EquipmentBatch& b) {
  CountingSubmitter submitter;
  submit_equipment_batch(b, submitter);
  return submitter.draw_count;
}

} // namespace

namespace {

DrawContext create_test_context() {
  DrawContext ctx;
  ctx.model.setToIdentity();
  ctx.backend = nullptr;
  ctx.entity = nullptr;
  return ctx;
}

BodyFrames create_test_frames() {
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

HumanoidPalette create_test_palette() {
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

    register_built_in_equipment();

    ctx = create_test_context();
    frames = create_test_frames();
    palette = create_test_palette();
    anim.inputs.time = 0.0F;
    anim.inputs.movement_state = Render::Creature::MovementAnimationState::Idle;
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
  EXPECT_GE(draw_count_of(submitter), 20);
}

TEST_F(HelmetRenderersTest, RomanHeavyHelmetHandlesZeroHeadRadius) {
  RomanHeavyHelmetRenderer helmet;
  frames.head.radius = 0.0F;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_EQ(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, RomanLightHelmetRendersWithValidFrames) {
  RomanLightHelmetRenderer helmet;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_GT(submitter.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(submitter), 8);
}

TEST_F(HelmetRenderersTest, RomanLightHelmetHandlesZeroHeadRadius) {
  RomanLightHelmetRenderer helmet;
  frames.head.radius = 0.0F;

  helmet.render(ctx, frames, palette, anim, submitter);

  EXPECT_EQ(draw_count_of(submitter), 0);
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

  EXPECT_EQ(draw_count_of(submitter), 0);
}

TEST_F(HelmetRenderersTest, HelmetsRegisteredInEquipmentRegistry) {
  auto& registry = EquipmentRegistry::instance();

  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "carthage_heavy"));
  EXPECT_NE(registry.resolve_handle(EquipmentCategory::Helmet, "carthage_heavy"),
            k_invalid_equipment_handle);

  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "carthage_light"));
  EXPECT_NE(registry.resolve_handle(EquipmentCategory::Helmet, "carthage_light"),
            k_invalid_equipment_handle);

  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "roman_heavy"));
  EXPECT_NE(registry.resolve_handle(EquipmentCategory::Helmet, "roman_heavy"),
            k_invalid_equipment_handle);

  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "roman_light"));
  EXPECT_NE(registry.resolve_handle(EquipmentCategory::Helmet, "roman_light"),
            k_invalid_equipment_handle);

  EXPECT_TRUE(registry.has(EquipmentCategory::Helmet, "headwrap"));
  EXPECT_NE(registry.resolve_handle(EquipmentCategory::Helmet, "headwrap"),
            k_invalid_equipment_handle);
}

TEST_F(HelmetRenderersTest, HelmetsUseHeadFrameCoordinates) {

  frames.head.origin = QVector3D(1.0F, 2.0F, 3.0F);
  frames.head.right = QVector3D(0.0F, 1.0F, 0.0F);
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

  RomanHeavyHelmetRenderer heavy_roman_helmet;
  MockSubmitter submitter_heavy_roman;
  heavy_roman_helmet.render(ctx, frames, palette, anim, submitter_heavy_roman);
  EXPECT_GE(draw_count_of(submitter_heavy_roman), 20);

  RomanLightHelmetRenderer light_roman_helmet;
  MockSubmitter submitter_light_roman;
  light_roman_helmet.render(ctx, frames, palette, anim, submitter_light_roman);
  EXPECT_GE(draw_count_of(submitter_light_roman), 8);

  HeadwrapRenderer headwrap;
  MockSubmitter submitter2;
  headwrap.render(ctx, frames, palette, anim, submitter2);

  EXPECT_GT(draw_count_of(submitter2), 0);
}

TEST_F(HelmetRenderersTest, HelmetsApplyVisibleVerticalOffset) {
  auto const expected_origin =
      frames.head.origin +
      frames.head.up *
          (k_helmet_local_offset.y() * frames.head.radius * k_helmet_uniform_scale);

  CarthageHeavyHelmetRenderer carthage_heavy;
  carthage_heavy.render(ctx, frames, palette, anim, submitter);
  ASSERT_FALSE(submitter.archetypes.empty());
  auto const carthage_heavy_origin =
      submitter.archetypes.front().world.column(3).toVector3D();
  EXPECT_NEAR(carthage_heavy_origin.x(), expected_origin.x(), 1.0e-5F);
  EXPECT_NEAR(carthage_heavy_origin.y(), expected_origin.y(), 1.0e-5F);
  EXPECT_NEAR(carthage_heavy_origin.z(), expected_origin.z(), 1.0e-5F);
  EXPECT_GT(carthage_heavy_origin.y(), frames.head.origin.y());

  MockSubmitter headwrap_submitter;
  HeadwrapRenderer headwrap;
  headwrap.render(ctx, frames, palette, anim, headwrap_submitter);
  ASSERT_FALSE(headwrap_submitter.archetypes.empty());
  auto const headwrap_origin =
      headwrap_submitter.archetypes.front().world.column(3).toVector3D();
  EXPECT_NEAR(headwrap_origin.x(), expected_origin.x(), 1.0e-5F);
  EXPECT_NEAR(headwrap_origin.y(), expected_origin.y(), 1.0e-5F);
  EXPECT_NEAR(headwrap_origin.z(), expected_origin.z(), 1.0e-5F);
}

TEST_F(HelmetRenderersTest, HelmetsApplyUniformScale) {
  auto const expected_scale = frames.head.radius * k_helmet_uniform_scale;

  RomanHeavyHelmetRenderer heavy_helmet;
  heavy_helmet.render(ctx, frames, palette, anim, submitter);
  ASSERT_FALSE(submitter.archetypes.empty());
  auto const heavy_world = submitter.archetypes.front().world;
  EXPECT_NEAR(heavy_world.column(0).toVector3D().length(), expected_scale, 1.0e-5F);
  EXPECT_NEAR(heavy_world.column(1).toVector3D().length(), expected_scale, 1.0e-5F);
  EXPECT_NEAR(heavy_world.column(2).toVector3D().length(), expected_scale, 1.0e-5F);

  MockSubmitter headwrap_submitter;
  HeadwrapRenderer headwrap;
  headwrap.render(ctx, frames, palette, anim, headwrap_submitter);
  ASSERT_FALSE(headwrap_submitter.archetypes.empty());
  auto const headwrap_world = headwrap_submitter.archetypes.front().world;
  EXPECT_NEAR(headwrap_world.column(0).toVector3D().length(), expected_scale, 1.0e-5F);
  EXPECT_NEAR(headwrap_world.column(1).toVector3D().length(), expected_scale, 1.0e-5F);
  EXPECT_NEAR(headwrap_world.column(2).toVector3D().length(), expected_scale, 1.0e-5F);
}
