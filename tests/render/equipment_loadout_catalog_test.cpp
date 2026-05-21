#include <array>
#include <gtest/gtest.h>

#include "render/creature/archetype_registry.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/humanoid_equipment_archetype.h"

namespace {

using Render::GL::k_invalid_equipment_handle;
using LoadoutHandleMember =
    Render::GL::EquipmentHandle Render::GL::Nation::ResolvedEquipmentLoadout::*;

class EquipmentLoadoutCatalogTest : public ::testing::Test {
protected:
  void SetUp() override { Render::GL::register_built_in_equipment(); }
};

TEST_F(EquipmentLoadoutCatalogTest, RomanMountedKnightLoadoutFromData) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/horse_swordsman");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.sword, "sword_roman");
  EXPECT_EQ(loadout.ids.shield, "roman_scutum");
  EXPECT_EQ(loadout.ids.armor, "roman_heavy_armor");
  EXPECT_EQ(loadout.ids.shoulder, "roman_shoulder_cover_cavalry");
  EXPECT_EQ(loadout.ids.horse_saddle, "roman_horse_saddle");
  EXPECT_EQ(loadout.ids.horse_barding, "horse_scale_barding");
  EXPECT_EQ(loadout.ids.horse_crupper, "horse_crupper");
  EXPECT_NE(loadout.sword_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.shield_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_saddle_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_barding_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_crupper_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, RomanHorseArcherLoadoutUsesRomanCloakHandle) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/horse_archer");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.bow, "bow_roman");
  EXPECT_EQ(loadout.ids.quiver, "quiver");
  EXPECT_EQ(loadout.ids.armor, "roman_light_armor");
  EXPECT_EQ(loadout.ids.cloak, "cloak_roman");
  EXPECT_NE(loadout.bow_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.quiver_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.cloak_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, RomanArcherLoadoutResolvesFootEquipment) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/archer");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.bow, "bow_roman");
  EXPECT_EQ(loadout.ids.quiver, "quiver_roman");
  EXPECT_EQ(loadout.ids.helmet, "roman_light");
  EXPECT_EQ(loadout.ids.greaves, "roman_greaves");
  EXPECT_EQ(loadout.ids.armor, "roman_light_armor");
  EXPECT_EQ(loadout.ids.cloak, "cloak_roman");
  EXPECT_NE(loadout.bow_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.quiver_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.helmet_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.greaves_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.cloak_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, RomanSpearmanLoadoutResolvesGreavesAndShoulder) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/spearman");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.spear, "spear");
  EXPECT_EQ(loadout.ids.helmet, "roman_heavy");
  EXPECT_EQ(loadout.ids.greaves, "roman_greaves");
  EXPECT_EQ(loadout.ids.armor, "roman_light_armor");
  EXPECT_EQ(loadout.ids.shoulder, "roman_shoulder_cover");
  EXPECT_NE(loadout.spear_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.helmet_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.greaves_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.shoulder_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest,
       CarthageSwordsmanLoadoutResolvesFootShieldAndArmor) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/swordsman");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.sword, "sword_carthage");
  EXPECT_EQ(loadout.ids.shield, "shield_carthage");
  EXPECT_EQ(loadout.ids.helmet, "carthage_heavy");
  EXPECT_EQ(loadout.ids.armor, "armor_heavy_carthage");
  EXPECT_EQ(loadout.ids.shoulder, "carthage_shoulder_cover");
  EXPECT_NE(loadout.sword_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.shield_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.helmet_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.shoulder_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, RomanHealerLoadoutResolvesSupportArmor) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/healer");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.helmet, "roman_light");
  EXPECT_EQ(loadout.ids.armor, "roman_light_armor");
  EXPECT_NE(loadout.helmet_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, CarthageHealerLoadoutResolvesSupportArmor) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/healer");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.armor, "armor_light_carthage");
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, IronSepulcherLoadoutsExposeBonesAndPriestCloak) {
  const auto swordsman = Render::GL::Nation::resolve_equipment_loadout(
      "troops/iron_sepulcher/skeleton_swordsman");
  ASSERT_TRUE(swordsman.found);
  EXPECT_EQ(swordsman.ids.sword, "sword_sepulcher");
  EXPECT_TRUE(swordsman.ids.shield.empty());
  EXPECT_NE(swordsman.sword_handle, k_invalid_equipment_handle);
  EXPECT_EQ(swordsman.shield_handle, k_invalid_equipment_handle);

  const auto priest = Render::GL::Nation::resolve_equipment_loadout(
      "troops/iron_sepulcher/grave_priest");
  ASSERT_TRUE(priest.found);
  EXPECT_EQ(priest.ids.cloak, "cloak_sepulcher");
  EXPECT_NE(priest.cloak_handle, k_invalid_equipment_handle);
  EXPECT_EQ(priest.armor_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, RomanBuilderLoadoutResolvesSupportGear) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/builder");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.helmet, "roman_light");
  EXPECT_EQ(loadout.ids.tool_belt, "tool_belt_roman");
  EXPECT_EQ(loadout.ids.work_apron, "work_apron_roman");
  EXPECT_EQ(loadout.ids.arm_guards, "arm_guards_roman");
  EXPECT_NE(loadout.helmet_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.tool_belt_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.work_apron_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.arm_guards_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, CarthageBuilderLoadoutResolvesSupportGear) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/builder");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.tool_belt, "tool_belt_carthage");
  EXPECT_EQ(loadout.ids.work_apron, "work_apron_carthage");
  EXPECT_EQ(loadout.ids.arm_guards, "arm_guards_carthage");
  EXPECT_NE(loadout.tool_belt_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.work_apron_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.arm_guards_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, RomanCivilianLoadoutResolvesHandleOnlyGarments) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/civilian");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.armor, "builder_tunic_roman");
  EXPECT_EQ(loadout.ids.cloak, "roman_civilian_mantle");
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.cloak_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, CarthageCivilianLoadoutResolvesHandleOnlyGarments) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/civilian");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.helmet, "headwrap_carthage_civilian");
  EXPECT_EQ(loadout.ids.armor, "carthage_robes");
  EXPECT_EQ(loadout.ids.cloak, "carthage_civilian_sash");
  EXPECT_NE(loadout.helmet_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.cloak_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, CarthageHorseArcherLoadoutResolvesHandles) {
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_archer");

  ASSERT_TRUE(loadout.found);
  EXPECT_EQ(loadout.ids.bow, "bow_carthage");
  EXPECT_EQ(loadout.ids.quiver, "quiver");
  EXPECT_EQ(loadout.ids.armor, "armor_light_carthage");
  EXPECT_EQ(loadout.ids.cloak, "cloak_carthage");
  EXPECT_EQ(loadout.ids.horse_saddle, "carthage_horse_saddle");
  EXPECT_EQ(loadout.ids.horse_bridle, "horse_bridle");
  EXPECT_EQ(loadout.ids.horse_decoration, "horse_saddle_bag");
  EXPECT_NE(loadout.bow_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.quiver_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.armor_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.cloak_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_saddle_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_bridle_handle, k_invalid_equipment_handle);
  EXPECT_NE(loadout.horse_decoration_handle, k_invalid_equipment_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, MountedNationsUseDistinctHorseSaddles) {
  const auto roman =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/horse_archer");
  const auto carthage =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_archer");

  ASSERT_TRUE(roman.found);
  ASSERT_TRUE(carthage.found);
  EXPECT_EQ(roman.ids.horse_saddle, "roman_horse_saddle");
  EXPECT_EQ(carthage.ids.horse_saddle, "carthage_horse_saddle");
  EXPECT_NE(roman.horse_saddle_handle, k_invalid_equipment_handle);
  EXPECT_NE(carthage.horse_saddle_handle, k_invalid_equipment_handle);
  EXPECT_NE(roman.horse_saddle_handle, carthage.horse_saddle_handle);
}

TEST_F(EquipmentLoadoutCatalogTest, CommanderLoadoutsResolveAllExpectedHandles) {
  struct CommanderLoadoutExpectation {
    const char* renderer_key;
    const char* primary_id;
    LoadoutHandleMember primary_handle;
    std::array<LoadoutHandleMember, 5> extra_handles;
  };

  const std::array<CommanderLoadoutExpectation, 6> expectations{{
      {"troops/roman/commanders/fabius_maximus",
       "spear_fabius",
       &Render::GL::Nation::ResolvedEquipmentLoadout::spear_handle,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::greaves_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::shoulder_handle,
        nullptr}},
      {"troops/roman/commanders/scipio_africanus",
       "sword_scipio",
       &Render::GL::Nation::ResolvedEquipmentLoadout::sword_handle,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::shield_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::greaves_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        nullptr}},
      {"troops/roman/commanders/marcellus",
       "bow_marcellus",
       &Render::GL::Nation::ResolvedEquipmentLoadout::bow_handle,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::quiver_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::cloak_handle,
        nullptr}},
      {"troops/carthage/commanders/hanno_the_great",
       "spear_hanno",
       &Render::GL::Nation::ResolvedEquipmentLoadout::spear_handle,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::cloak_handle,
        nullptr,
        nullptr}},
      {"troops/carthage/commanders/hasdrubal_barca",
       "bow_hasdrubal",
       &Render::GL::Nation::ResolvedEquipmentLoadout::bow_handle,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::quiver_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        nullptr,
        nullptr}},
      {"troops/carthage/commanders/hannibal_barca",
       "sword_hannibal",
       &Render::GL::Nation::ResolvedEquipmentLoadout::sword_handle,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::shield_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::shoulder_handle,
        nullptr}},
  }};

  for (const auto& expectation : expectations) {
    SCOPED_TRACE(expectation.renderer_key);
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout(expectation.renderer_key);
    ASSERT_TRUE(loadout.found);
    EXPECT_NE(loadout.*(expectation.primary_handle), k_invalid_equipment_handle)
        << expectation.primary_id;
    for (auto extra_handle : expectation.extra_handles) {
      if (extra_handle == nullptr) {
        continue;
      }
      EXPECT_NE(loadout.*extra_handle, k_invalid_equipment_handle);
    }
  }
}

TEST_F(EquipmentLoadoutCatalogTest, CommanderLoadoutsBakeDistinctArchetypesFromBase) {
  struct CommanderBakeExpectation {
    const char* renderer_key;
    Render::Creature::ArchetypeId base_archetype;
    std::array<LoadoutHandleMember, 7> handles;
  };

  const std::array<CommanderBakeExpectation, 6> expectations{{
      {"troops/roman/commanders/fabius_maximus",
       Render::Creature::ArchetypeRegistry::k_humanoid_base,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::greaves_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::shoulder_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::spear_handle,
        nullptr,
        nullptr}},
      {"troops/roman/commanders/scipio_africanus",
       Render::Creature::ArchetypeRegistry::k_humanoid_base,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::greaves_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::shield_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::sword_handle,
        nullptr,
        nullptr}},
      {"troops/roman/commanders/marcellus",
       Render::Creature::ArchetypeRegistry::k_humanoid_base,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::quiver_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::bow_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::cloak_handle,
        nullptr,
        nullptr}},
      {"troops/carthage/commanders/hanno_the_great",
       Render::Creature::ArchetypeRegistry::k_humanoid_base,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::spear_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::cloak_handle,
        nullptr,
        nullptr,
        nullptr}},
      {"troops/carthage/commanders/hasdrubal_barca",
       Render::Creature::ArchetypeRegistry::k_humanoid_base,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::quiver_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::bow_handle,
        nullptr,
        nullptr,
        nullptr}},
      {"troops/carthage/commanders/hannibal_barca",
       Render::Creature::ArchetypeRegistry::k_humanoid_base,
       {&Render::GL::Nation::ResolvedEquipmentLoadout::helmet_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::shield_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::shoulder_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::armor_handle,
        &Render::GL::Nation::ResolvedEquipmentLoadout::sword_handle,
        nullptr,
        nullptr}},
  }};

  for (const auto& expectation : expectations) {
    SCOPED_TRACE(expectation.renderer_key);
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout(expectation.renderer_key);
    ASSERT_TRUE(loadout.found);

    std::array<Render::GL::EquipmentHandle, 7> handles{};
    for (std::size_t i = 0; i < expectation.handles.size(); ++i) {
      handles[i] = expectation.handles[i] == nullptr
                       ? k_invalid_equipment_handle
                       : loadout.*(expectation.handles[i]);
    }

    const auto archetype = Render::GL::resolve_humanoid_equipment_archetype(
        expectation.renderer_key, expectation.base_archetype, handles);
    EXPECT_NE(archetype, Render::Creature::k_invalid_archetype);
    EXPECT_NE(archetype, expectation.base_archetype);
  }
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
