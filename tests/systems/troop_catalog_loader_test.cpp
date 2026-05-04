#include "game/units/troop_catalog.h"
#include "game/units/troop_catalog_loader.h"

#include <gtest/gtest.h>

namespace {

TEST(TroopCatalogLoader, ElephantKeepsGroundOffsetWithoutRingYOffset) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const *elephant = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Elephant);
  ASSERT_NE(elephant, nullptr);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_size, 1.3F);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_ground_offset, 0.6F);
}

TEST(TroopCatalogLoader, CavalryProfilesUseLargerSelectionRings) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  for (auto const troop_type : {Game::Units::TroopType::MountedKnight,
                                Game::Units::TroopType::HorseArcher,
                                Game::Units::TroopType::HorseSpearman}) {
    auto const *troop =
        Game::Units::TroopCatalog::instance().get_class(troop_type);
    ASSERT_NE(troop, nullptr);
    EXPECT_FLOAT_EQ(troop->visuals.selection_ring_size, 2.2F);
  }
}

} // namespace
