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

TEST_F(ArmorRendererTest, TunicRegisteredInRegistry) {
  auto tunic = registry->get(EquipmentCategory::Armor, "tunic");
  ASSERT_NE(tunic, nullptr);
}

TEST_F(ArmorRendererTest, HeavyTunicRegisteredInRegistry) {
  auto heavy_tunic = registry->get(EquipmentCategory::Armor, "heavy_tunic");
  ASSERT_NE(heavy_tunic, nullptr);
}

TEST_F(ArmorRendererTest, LightTunicRegisteredInRegistry) {
  auto light_tunic = registry->get(EquipmentCategory::Armor, "light_tunic");
  ASSERT_NE(light_tunic, nullptr);
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

TEST_F(ArmorRendererTest, TunicRendererWithDefaultConfig) {
  auto tunic = std::make_shared<TunicRenderer>();
  ASSERT_NE(tunic, nullptr);
}

TEST_F(ArmorRendererTest, ArmorCategoryIsDistinct) {
  auto helmet = registry->get(EquipmentCategory::Helmet, "kingdom_heavy");
  auto armor = registry->get(EquipmentCategory::Armor, "heavy_tunic");
  auto weapon = registry->get(EquipmentCategory::Weapon, "bow");

  ASSERT_NE(helmet, nullptr);
  ASSERT_NE(armor, nullptr);
  ASSERT_NE(weapon, nullptr);

  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "kingdom_heavy"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "heavy_tunic"));
}
