#include <gtest/gtest.h>

#include "game/units/troop_catalog.h"
#include "game/units/troop_catalog_loader.h"
#include "game/units/troop_config.h"

namespace {

TEST(TroopCatalogLoader, ElephantKeepsGroundOffsetWithoutRingYOffset) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* elephant =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Elephant);
  ASSERT_NE(elephant, nullptr);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_size, 1.5F);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_ground_offset, 0.6F);
}

TEST(TroopCatalogLoader, CivilianStaysInLoadedCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* civilian =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Civilian);
  ASSERT_NE(civilian, nullptr);
  EXPECT_EQ(civilian->display_name, "Civilian");
  EXPECT_EQ(civilian->visuals.renderer_id, "troops/roman/civilian");
  EXPECT_EQ(civilian->production.cost, 8);
}

TEST(TroopCatalogLoader, FrontlineInfantryKeepConfiguredFormationSpacing) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* swordsman = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Swordsman);
  auto const* spearman =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Spearman);
  ASSERT_NE(swordsman, nullptr);
  ASSERT_NE(spearman, nullptr);

  EXPECT_FLOAT_EQ(swordsman->visuals.formation_spacing, 1.05F);
  EXPECT_FLOAT_EQ(spearman->visuals.formation_spacing, 1.05F);
  EXPECT_FLOAT_EQ(Game::Units::TroopConfig::instance().get_formation_spacing(
                      Game::Units::TroopType::Swordsman),
                  1.05F);
  EXPECT_FLOAT_EQ(Game::Units::TroopConfig::instance().get_formation_spacing(
                      Game::Units::TroopType::Spearman),
                  1.05F);
}

TEST(TroopCatalogLoader, ProductionResourceCostsLoadForEconomyUnits) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* archer =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Archer);
  auto const* builder =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Builder);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(builder, nullptr);

  EXPECT_EQ(archer->production.resource_costs.get(Game::Systems::ResourceType::Wood),
            6);
  EXPECT_EQ(archer->production.resource_costs.get(Game::Systems::ResourceType::Stone),
            0);
  EXPECT_EQ(builder->production.resource_costs.get(Game::Systems::ResourceType::Wood),
            0);
  EXPECT_TRUE(builder->production.resource_costs.empty());
}

TEST(TroopCatalogLoader, IronSepulcherTroopsLoadFromCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* skeleton_swordsman = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::SkeletonSwordsman);
  auto const* skeleton_archer = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::SkeletonArcher);
  auto const* grave_priest = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::GravePriest);
  ASSERT_NE(skeleton_swordsman, nullptr);
  ASSERT_NE(skeleton_archer, nullptr);
  ASSERT_NE(grave_priest, nullptr);

  EXPECT_EQ(skeleton_swordsman->production.cost, 0);
  EXPECT_FALSE(skeleton_swordsman->combat.can_ranged);
  EXPECT_EQ(skeleton_swordsman->visuals.renderer_id,
            "troops/iron_sepulcher/skeleton_swordsman");

  EXPECT_EQ(skeleton_archer->production.cost, 0);
  EXPECT_TRUE(skeleton_archer->combat.can_ranged);
  EXPECT_EQ(skeleton_archer->visuals.renderer_id,
            "troops/iron_sepulcher/skeleton_archer");

  EXPECT_EQ(grave_priest->production.cost, 0);
  EXPECT_TRUE(grave_priest->combat.can_ranged);
  EXPECT_EQ(grave_priest->visuals.renderer_id, "troops/iron_sepulcher/grave_priest");
}

TEST(TroopCatalogLoader, CommandersLoadFromCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* fabius = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::RomanLegionOrganizer);
  auto const* scipio = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::RomanVeteranConsul);
  auto const* marcellus = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::RomanFieldCommander);
  auto const* hanno = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::CarthageMercenaryBroker);
  auto const* hasdrubal = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::CarthageCavalryPatron);
  auto const* hannibal = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::CarthageElephantMaster);
  ASSERT_NE(fabius, nullptr);
  ASSERT_NE(scipio, nullptr);
  ASSERT_NE(marcellus, nullptr);
  ASSERT_NE(hanno, nullptr);
  ASSERT_NE(hasdrubal, nullptr);
  ASSERT_NE(hannibal, nullptr);

  EXPECT_EQ(fabius->visuals.renderer_id, "troops/roman/commanders/fabius_maximus");
  EXPECT_EQ(scipio->visuals.renderer_id, "troops/roman/commanders/scipio_africanus");
  EXPECT_EQ(marcellus->visuals.renderer_id, "troops/roman/commanders/marcellus");
  EXPECT_EQ(hanno->visuals.renderer_id, "troops/carthage/commanders/hanno_the_great");
  EXPECT_EQ(hasdrubal->visuals.renderer_id,
            "troops/carthage/commanders/hasdrubal_barca");
  EXPECT_EQ(hannibal->visuals.renderer_id, "troops/carthage/commanders/hannibal_barca");
}

} // namespace
