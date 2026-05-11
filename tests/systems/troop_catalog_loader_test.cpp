#include "game/units/troop_catalog.h"
#include "game/units/troop_catalog_loader.h"
#include "game/units/troop_config.h"

#include <gtest/gtest.h>

namespace {

TEST(TroopCatalogLoader, ElephantKeepsGroundOffsetWithoutRingYOffset) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const *elephant = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Elephant);
  ASSERT_NE(elephant, nullptr);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_size, 1.5F);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_ground_offset, 0.6F);
}

TEST(TroopCatalogLoader, CivilianStaysInLoadedCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const *civilian = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Civilian);
  ASSERT_NE(civilian, nullptr);
  EXPECT_EQ(civilian->display_name, "Civilian");
  EXPECT_EQ(civilian->visuals.renderer_id, "troops/roman/civilian");
  EXPECT_EQ(civilian->production.cost, 8);
}

TEST(TroopCatalogLoader, FrontlineInfantryKeepConfiguredFormationSpacing) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const *swordsman = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Swordsman);
  auto const *spearman = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Spearman);
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

} // namespace
