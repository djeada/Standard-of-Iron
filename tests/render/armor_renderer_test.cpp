#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/armor/tunic_renderer.h"
#include "render/equipment/equipment_registry.h"
#include "render/humanoid/rig.h"
#include <gtest/gtest.h>
#include <memory>

using namespace Render::GL;

class ArmorRendererTest : public ::testing::Test {
protected:
  void SetUp() override {
    registerBuiltInEquipment();
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
