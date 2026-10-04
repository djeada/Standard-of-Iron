#include <gtest/gtest.h>
#include <vector>

#include "game/render_bridge/minimap/unit_layer.h"

namespace {

using Game::Map::Minimap::MarkerClass;
using Game::Map::Minimap::UnitLayer;
using Game::Map::Minimap::UnitMarker;

auto marker(float x, float z, int owner, MarkerClass kind) -> UnitMarker {
  UnitMarker value;
  value.world_x = x;
  value.world_z = z;
  value.owner_id = owner;
  value.marker_class = kind;
  return value;
}

TEST(MinimapUnitLayerTest, MovingTroopsDoNotRepaintTheStructures) {
  UnitLayer layer;
  layer.init(96, 96, 96.0F, 96.0F);

  std::vector<UnitMarker> markers{marker(-20.0F, -20.0F, 1, MarkerClass::Stronghold),
                                  marker(15.0F, 10.0F, 2, MarkerClass::Tower),
                                  marker(5.0F, 5.0F, 1, MarkerClass::MinorStructure),
                                  marker(0.0F, 0.0F, 1, MarkerClass::Troop)};
  layer.update(markers);
  EXPECT_EQ(layer.structure_redraws(), 1U);
  const QImage before = layer.get_image().copy();

  for (int step = 1; step <= 5; ++step) {
    markers.back().world_x = static_cast<float>(step) * 3.0F;
    layer.update(markers);
  }
  EXPECT_EQ(layer.structure_redraws(), 1U)
      << "troop movement must not repaint every tower and stronghold";
  EXPECT_NE(layer.get_image(), before) << "the troop dot itself still moves";

  markers.front().owner_id = 2;
  layer.update(markers);
  EXPECT_EQ(layer.structure_redraws(), 2U) << "a captured stronghold repaints";

  markers.back().is_selected = true;
  layer.update(markers);
  EXPECT_EQ(layer.structure_redraws(), 2U) << "selecting a troop is a dynamic change";
}

TEST(MinimapUnitLayerTest, TheCompositeMatchesAFreshLayerAfterTroopMoves) {
  std::vector<UnitMarker> markers{marker(-20.0F, -20.0F, 1, MarkerClass::Stronghold),
                                  marker(15.0F, 10.0F, 2, MarkerClass::Tower),
                                  marker(5.0F, 5.0F, 1, MarkerClass::MinorStructure),
                                  marker(0.0F, 0.0F, 1, MarkerClass::Troop)};
  UnitLayer reused;
  reused.init(96, 96, 96.0F, 96.0F);
  reused.update(markers);
  markers.back().world_x = 12.0F;
  markers.back().is_selected = true;
  reused.update(markers);

  UnitLayer fresh;
  fresh.init(96, 96, 96.0F, 96.0F);
  fresh.update(markers);

  EXPECT_EQ(reused.get_image(), fresh.get_image())
      << "cached structure layers composite to the same pixels as a full repaint";
}

} // namespace
