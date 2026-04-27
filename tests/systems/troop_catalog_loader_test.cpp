#include "game/units/troop_catalog.h"
#include "game/units/troop_catalog_loader.h"

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

} // namespace
