#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/armor/tunic_renderer.h"
#include "render/equipment/equipment_registry.h"
#include "render/humanoid/rig.h"
#include <QVector3D>
#include <gtest/gtest.h>
#include <memory>

using namespace Render::GL;

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

TEST_F(ArmorRendererTest, TunicRendererCreation) {
  TunicConfig config;
  config.torso_scale = 1.1F;
  config.include_pauldrons = true;
  config.include_gorget = true;
  config.include_belt = true;

  auto tunic = std::make_shared<TunicRenderer>(config);
  ASSERT_NE(tunic, nullptr);
}

TEST_F(ArmorRendererTest, ArmorCategoryIsDistinct) {
  auto helmet = registry->get(EquipmentCategory::Helmet, "roman_heavy");
  auto armor = registry->get(EquipmentCategory::Armor, "roman_heavy_armor");
  auto weapon = registry->get(EquipmentCategory::Weapon, "bow");

  ASSERT_NE(helmet, nullptr);
  ASSERT_NE(armor, nullptr);
  ASSERT_NE(weapon, nullptr);

  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "roman_heavy"));
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

  ASSERT_FALSE(batch.meshes.empty());
  QVector3D const mesh_front =
      batch.meshes.front().model.mapVector(QVector3D(1.0F, 0.0F, 0.0F))
          .normalized();
  EXPECT_GT(QVector3D::dotProduct(mesh_front, frames.torso.forward), 0.95F);
}
