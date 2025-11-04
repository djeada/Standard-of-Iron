#include "render/equipment/armor/carthage_armor.h"
#include "render/equipment/armor/kingdom_armor.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/armor/tunic_renderer.h"
#include "render/equipment/armor/armor_light.h"
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

// Test nation-specific heavy armor
TEST_F(ArmorRendererTest, KingdomHeavyArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "kingdom_heavy_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, KingdomLightArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "kingdom_light_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, RomanHeavyArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "roman_heavy_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, RomanLightArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "roman_light_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, CarthageHeavyArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "carthage_heavy_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, CarthageLightArmorRegistered) {
  auto armor = registry->get(EquipmentCategory::Armor, "carthage_light_armor");
  ASSERT_NE(armor, nullptr);
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
  auto helmet = registry->get(EquipmentCategory::Helmet, "kingdom_heavy");
  auto armor = registry->get(EquipmentCategory::Armor, "kingdom_heavy_armor");
  auto weapon = registry->get(EquipmentCategory::Weapon, "bow");

  ASSERT_NE(helmet, nullptr);
  ASSERT_NE(armor, nullptr);
  ASSERT_NE(weapon, nullptr);

  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "kingdom_heavy"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "kingdom_heavy_armor"));
}

TEST_F(ArmorRendererTest, CarthageArcherLightArmorRegistered) {
  auto armor =
      registry->get(EquipmentCategory::Armor, "carthage_archer_light_armor");
  ASSERT_NE(armor, nullptr);
}

TEST_F(ArmorRendererTest, CarthageArcherArmorDistinctFromStandardArmor) {
  auto standard_light =
      registry->get(EquipmentCategory::Armor, "carthage_light_armor");
  auto archer_light =
      registry->get(EquipmentCategory::Armor, "carthage_archer_light_armor");

  ASSERT_NE(standard_light, nullptr);
  ASSERT_NE(archer_light, nullptr);

  EXPECT_NE(standard_light, archer_light);
}
