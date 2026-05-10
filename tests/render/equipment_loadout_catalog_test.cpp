#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/equipment/equipment_registry.h"
#include <gtest/gtest.h>

namespace {

using Render::GL::k_invalid_equipment_handle;

class EquipmentLoadoutCatalogTest : public ::testing::Test {
protected:
  void SetUp() override { Render::GL::register_built_in_equipment(); }
};

TEST_F(EquipmentLoadoutCatalogTest, RomanMountedKnightLoadoutFromData) {
  const auto loadout = Render::GL::Nation::resolve_equipment_loadout(
      "troops/roman/horse_swordsman");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.sword, "sword_roman");
  EXPECT_EQ(loadout.ids.shield, "roman_scutum");
  EXPECT_EQ(loadout.ids.armor, "roman_heavy_armor");
  EXPECT_EQ(loadout.ids.shoulder, "roman_shoulder_cover_cavalry");
  EXPECT_EQ(loadout.ids.horse_barding, "horse_scale_barding");
  EXPECT_EQ(loadout.ids.horse_crupper, "horse_crupper");
  EXPECT_NE(loadout.sword_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.shield_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_barding_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_crupper_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, CarthageHorseArcherLoadoutResolvesHandles) {
  const auto loadout = Render::GL::Nation::resolve_equipment_loadout(
      "troops/carthage/horse_archer");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.bow, "bow_carthage");
  EXPECT_EQ(loadout.ids.quiver, "quiver");
  EXPECT_EQ(loadout.ids.armor, "armor_light_carthage");
  EXPECT_EQ(loadout.ids.cloak, "cloak_carthage");
  EXPECT_EQ(loadout.ids.horse_bridle, "horse_bridle");
  EXPECT_EQ(loadout.ids.horse_decoration, "horse_saddle_bag");
  EXPECT_NE(loadout.bow_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.quiver_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.cloak_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_bridle_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_decoration_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, MissingLoadoutReturnsNotFound) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/unknown/unit");

  EXPECT_FALSE(loadout.found);
  EXPECT_EQ(loadout.bow_handle, k_invalid_equipment_handle);
  EXPECT_EQ(loadout.sword_handle, k_invalid_equipment_handle);
  EXPECT_TRUE(loadout.ids.bow.empty());
  EXPECT_TRUE(loadout.ids.sword.empty());
}

} // namespace
