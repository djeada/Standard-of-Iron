#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/equipment/armor/carthage_shoulder_cover.h"
#include "render/equipment/armor/cloak_renderer.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/armor/roman_greaves.h"
#include "render/equipment/armor/roman_shoulder_cover.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include <QVector3D>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace Render::GL;

namespace {

class CountingSubmitter : public ISubmitter {
public:
  void mesh(Mesh *, const QMatrix4x4 &, const QVector3D &, Texture * = nullptr,
            float = 1.0F, int = 0) override {
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
                      float = 1.0F) override {}

  int draw_count{0};
};

class MeshCaptureSubmitter : public ISubmitter {
public:
  void mesh(Mesh *, const QMatrix4x4 &model, const QVector3D &,
            Texture * = nullptr, float = 1.0F, int = 0) override {
    models.push_back(model);
  }

  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float = 1.0F) override {}

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
                      float = 1.0F) override {}

  std::vector<QMatrix4x4> models;
};

inline int draw_count_of(const EquipmentBatch &batch) {
  CountingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  return submitter.draw_count;
}

} // namespace

class ArmorRendererTest : public ::testing::Test {
protected:
  void SetUp() override {
    register_built_in_equipment();
    registry = &EquipmentRegistry::instance();
  }

  EquipmentRegistry *registry = nullptr;
};

TEST_F(ArmorRendererTest, RomanHeavyArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "roman_heavy_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, RomanLightArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "roman_light_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, ToolBeltRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "tool_belt_roman");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, ArmGuardsRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "arm_guards");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, WorkApronRegistered) {
  auto roman = registry->get(EquipmentCategory::Armor, "work_apron_roman");
  auto carthage =
      registry->get(EquipmentCategory::Armor, "work_apron_carthage");
  ASSERT_NE(roman, nullptr);
  ASSERT_NE(carthage, nullptr);
}

TEST_F(ArmorRendererTest, RomanShoulderCoverRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "roman_shoulder_cover");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, CarthageShoulderCoverRegistered) {
  auto armor =
      registry->get(EquipmentCategory::Armor, "carthage_shoulder_cover");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, RomanGreavesRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "roman_greaves");
  ASSERT_NE(armor, nullptr);
}

// Test new separate Carthaginian archer armor files
TEST_F(ArmorRendererTest, ArmorLightCarthageRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "armor_light_carthage");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, ArmorHeavyCarthageRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "armor_heavy_carthage");
  ASSERT_NE(armor, nullptr);
}

// Verify both new armor variants share the same helmet
TEST_F(ArmorRendererTest, CarthageArcherArmorSharesHelmet) {
  auto light_armor =
      registry->get(EquipmentCategory::Armor, "armor_light_carthage");
  auto heavy_armor =
      registry->get(EquipmentCategory::Armor, "armor_heavy_carthage");
  auto shared_helmet =
      registry->get(EquipmentCategory::Helmet, "carthage_light");

  ASSERT_NE(light_armor, nullptr);
  ASSERT_NE(heavy_armor, nullptr);
  ASSERT_NE(shared_helmet, nullptr);
}

TEST_F(ArmorRendererTest, CloakRendererCreation) {
  CloakConfig config;
  config.length_scale = 1.1F;
  config.width_scale = 1.05F;

  auto cloak = std::make_shared<CloakRenderer>(config);
  ASSERT_NE(cloak, nullptr);
}

TEST_F(ArmorRendererTest, ArmorCategoryIsDistinct) {
  auto helmet = registry->get(EquipmentCategory::Helmet, "carthage_heavy");
  auto armor = registry->get(EquipmentCategory::Armor, "roman_heavy_armor");
  auto weapon = registry->get(EquipmentCategory::Weapon, "bow");

  ASSERT_NE(helmet, nullptr);
  ASSERT_NE(armor, nullptr);
  ASSERT_NE(weapon, nullptr);

  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "carthage_heavy"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "roman_heavy_armor"));
}

TEST_F(ArmorRendererTest, TorsoArmorMeshFrontFacesBodyForward) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.1F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;

  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;

  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  RomanLightArmorRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  ASSERT_FALSE(batch.archetypes.empty());
  MeshCaptureSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  ASSERT_FALSE(submitter.models.empty());
  QVector3D const mesh_front = submitter.models.front()
                                   .mapVector(QVector3D(1.0F, 0.0F, 0.0F))
                                   .normalized();
  EXPECT_GT(QVector3D::dotProduct(mesh_front, frames.torso.forward), 0.95F);
}

TEST_F(ArmorRendererTest, RomanHeavyArmorRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  RomanHeavyArmorRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 7);
}

TEST_F(ArmorRendererTest, RomanLightArmorRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  RomanLightArmorRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 5);
}

TEST_F(ArmorRendererTest, ArmorLightCarthageRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  ArmorLightCarthageRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 5);
}

TEST_F(ArmorRendererTest, ArmorHeavyCarthageRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  ArmorHeavyCarthageRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 3);
}

TEST_F(ArmorRendererTest, CloakRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  CloakRenderer cloak;
  cloak.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 3);
}

TEST_F(ArmorRendererTest, ToolBeltRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.waist.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.waist.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.waist.radius = 0.28F;
  frames.waist.depth = 0.22F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  ToolBeltRenderer tool_belt;
  tool_belt.render(ctx, frames, palette, anim, batch);

  EXPECT_GT(batch.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(batch), 16);
}

TEST_F(ArmorRendererTest, RomanShoulderCoverRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  RomanShoulderCoverRenderer shoulder_cover;
  shoulder_cover.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 6);
}

TEST_F(ArmorRendererTest, CarthageShoulderCoverRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  CarthageShoulderCoverRenderer shoulder_cover;
  shoulder_cover.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 6);
}

TEST_F(ArmorRendererTest, RomanGreavesRenderThroughArchetypePath) {
  BodyFrames frames{};
  frames.shin_l.origin = QVector3D(-0.10F, 0.18F, 0.02F);
  frames.shin_l.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.shin_l.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shin_l.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.shin_l.radius = 0.045F;
  frames.shin_r.origin = QVector3D(0.10F, 0.18F, 0.02F);
  frames.shin_r.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.shin_r.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shin_r.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.shin_r.radius = 0.045F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  RomanGreavesRenderer greaves;
  greaves.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 6);
}

TEST_F(ArmorRendererTest, ArmGuardsRenderThroughArchetypePath) {
  BodyFrames frames{};
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.30F, 0.00F);
  frames.hand_l.origin = QVector3D(-0.42F, 0.98F, 0.12F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.30F, 0.00F);
  frames.hand_r.origin = QVector3D(0.42F, 0.98F, 0.12F);

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  ArmGuardsRenderer arm_guards;
  arm_guards.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 16);
}

TEST_F(ArmorRendererTest, WorkApronRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.waist.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.waist.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.waist.radius = 0.28F;
  frames.waist.depth = 0.22F;

  DrawContext ctx{};
  HumanoidPalette palette{};
  HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  WorkApronRenderer apron;
  apron.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 3U);
  EXPECT_EQ(draw_count_of(batch), 99);
}
