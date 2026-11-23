#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <set>
#include <string>

#include "game/systems/building_collision_registry.h"
#include "game/units/building_spawn_setup.h"
#include "game/visuals/building_asset_key.h"
#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/dock_authoring.h"
#include "render/entity/market_lot.h"
#include "render/entity/portico_authoring.h"
#include "render/geom/mesh.h"

namespace {

using Render::GL::BuildingState;
namespace Lot = Render::GL::MarketLot;

// The furthest a description reaches along x and how high it stands, in
// building-local units.
struct Extent {
  float reach_x{0.0F};
  float top{0.0F};
};

auto extent_of(const Render::GL::BuildingArchetypeDesc& desc,
               BuildingState state) -> Extent {
  Extent extent;
  const auto archetype = Render::GL::build_building_archetype(desc, state);
  for (const auto& draw :
       archetype.lods[static_cast<std::size_t>(Render::GL::RenderArchetypeLod::Full)]
           .draws) {
    if (draw.mesh == nullptr) {
      continue;
    }
    for (const auto& vertex : draw.mesh->get_vertices()) {
      const QVector3D point = draw.local_model.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
      extent.reach_x = std::max(extent.reach_x, std::abs(point.x()));
      extent.top = std::max(extent.top, point.y());
    }
  }
  return extent;
}

} // namespace

TEST(MarketStyles, EveryStyleHasARendererKeyForBothNations) {
  using Game::Systems::NationID;
  for (const char* style : {"portico", "basilica", "dock"}) {
    EXPECT_TRUE(Game::Visuals::is_building_visual_style("marketplace", style)) << style;
    EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                  NationID::RomanRepublic, "marketplace", style),
              std::string("troops/roman/marketplace_") + style);
    EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                  NationID::Carthage, "marketplace", style),
              std::string("troops/carthage/marketplace_") + style);
  }
  EXPECT_FALSE(Game::Visuals::is_building_visual_style("home", "dock"));
  EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                NationID::IronSepulcher, "marketplace", "dock"),
            "troops/iron_sepulcher/marketplace")
      << "a nation without the style draws its plain building";
}

TEST(MarketStyles, TheBakeCatalogHoldsEveryVariantUnderTheKindThatDrawsIt) {
  std::set<std::string> names;
  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    const bool portico = entry.name.find("_portico_variant_") != std::string::npos;
    const bool dock = entry.name.find("_dock_variant_") != std::string::npos;
    if (!portico && !dock) {
      continue;
    }
    names.insert(entry.name);
    ASSERT_EQ(entry.kinds.size(), 1U) << entry.name;
    const std::string nation = entry.name.starts_with("roman") ? "roman" : "carthage";
    const bool basilica =
        portico &&
        entry.name.ends_with(std::to_string(Render::GL::k_portico_basilica_variant));
    EXPECT_EQ(entry.kinds.front(),
              "troops/" + nation + "/marketplace_" +
                  (dock ? "dock" : (basilica ? "basilica" : "portico")))
        << entry.name;
    for (const BuildingState state :
         {BuildingState::Normal, BuildingState::Damaged, BuildingState::Destroyed}) {
      EXPECT_FALSE(entry.build(state).parts().empty()) << entry.name;
    }
  }
  EXPECT_EQ(names.size(),
            2U * static_cast<std::size_t>(Render::GL::k_portico_variant_count +
                                          Render::GL::k_dock_variant_count));
}

TEST(MarketLot, AStoaClosesUpOnTheMarketplaceFootprint) {
  const auto nav =
      Game::Systems::BuildingCollisionRegistry::get_building_size("marketplace");
  const QVector3D scale = Game::Units::building_transform_scale("marketplace");
  const float drawn_width = 2.0F * Lot::k_half * scale.x();
  EXPECT_LE(drawn_width, nav.width);
  EXPECT_GE(drawn_width, nav.width - 0.06F)
      << "two porticoes on neighbouring lots must meet, not leave a slot between";

  for (const auto* look :
       {&Render::GL::roman_portico_look(), &Render::GL::carthage_portico_look()}) {
    for (int variant = 0; variant < Render::GL::k_portico_variant_count; ++variant) {
      const Extent extent = extent_of(
          Render::GL::build_portico_desc(*look, BuildingState::Normal, variant),
          BuildingState::Normal);
      EXPECT_LE(extent.reach_x, Lot::k_half + 0.02F)
          << look->name_prefix << variant << " stands in its neighbour's lot";
    }
  }
}

TEST(MarketLot, TheBasilicaStandsWellOverThePlainWalk) {
  for (const auto* look :
       {&Render::GL::roman_portico_look(), &Render::GL::carthage_portico_look()}) {
    const float walk =
        extent_of(Render::GL::build_portico_desc(*look, BuildingState::Normal, 0),
                  BuildingState::Normal)
            .top;
    const float hall =
        extent_of(
            Render::GL::build_portico_desc(
                *look, BuildingState::Normal, Render::GL::k_portico_basilica_variant),
            BuildingState::Normal)
            .top;
    EXPECT_GT(hall, walk * 1.6F)
        << look->name_prefix
        << ": the long sides of a forum have to read as halls from across a city";
  }
}

TEST(MarketLot, ARunOfPorticoesSharesOneSeedHoweverLongItIs) {
  // Along x, facing +z.
  const std::uint32_t along_x = Render::GL::lot_run_seed(0.0F, -36.75F, 0.0F, 1.0F);
  for (int lot = 1; lot < 30; ++lot) {
    EXPECT_EQ(
        Render::GL::lot_run_seed(5.5F * static_cast<float>(lot), -36.75F, 0.0F, 1.0F),
        along_x)
        << "lot " << lot;
  }
  // Along z, facing +x: the same holds whichever way the run lies.
  const std::uint32_t along_z = Render::GL::lot_run_seed(-69.75F, 50.0F, 1.0F, 0.0F);
  for (int lot = 1; lot < 20; ++lot) {
    EXPECT_EQ(Render::GL::lot_run_seed(
                  -69.75F, 50.0F + (5.5F * static_cast<float>(lot)), 1.0F, 0.0F),
              along_z);
  }
  // The stoa across the square is another building.
  EXPECT_NE(Render::GL::lot_run_seed(0.0F, -5.25F, 0.0F, -1.0F), along_x);
  EXPECT_NE(Render::GL::lot_run_seed(0.0F, -31.25F, 0.0F, 1.0F), along_x);

  for (std::uint32_t seed = 0; seed < 64U; ++seed) {
    const int variant = Render::GL::portico_run_variant(seed * 2654435761U);
    EXPECT_GE(variant, 0);
    EXPECT_LT(variant, Render::GL::k_portico_walk_variants)
        << "a plain portico never draws the basilica";
  }
}

TEST(MarketLot, NeighbouringDocksNeverBothMoorAShip) {
  // A ship is longer than its lot: two side by side would lie in each other.
  constexpr float k_pitch = 5.5F;
  for (const bool along_x : {true, false}) {
    int ships = 0;
    int previous = -1;
    for (int lot = -20; lot <= 20; ++lot) {
      const float run = k_pitch * static_cast<float>(lot);
      const int variant = along_x ? Render::GL::dock_visual_variant(run, 47.25F)
                                  : Render::GL::dock_visual_variant(-83.75F, run);
      ASSERT_GE(variant, 0);
      ASSERT_LT(variant, Render::GL::k_dock_variant_count);
      EXPECT_FALSE(variant == 0 && previous == 0) << "lot " << lot;
      ships += variant == 0 ? 1 : 0;
      previous = variant;
    }
    EXPECT_GT(ships, 12) << "a quay with no ships at it is not a harbour";
  }

  for (const auto* look :
       {&Render::GL::roman_dock_look(), &Render::GL::carthage_dock_look()}) {
    const float ship =
        extent_of(Render::GL::build_dock_desc(*look, BuildingState::Normal, 0),
                  BuildingState::Normal)
            .reach_x;
    EXPECT_GT(ship, Lot::k_half) << "the ship is what overhangs";
    EXPECT_LT(ship, 3.0F * Lot::k_half)
        << "but never past the lot beyond its neighbour";
    for (int variant = 1; variant < Render::GL::k_dock_variant_count; ++variant) {
      EXPECT_LE(
          extent_of(Render::GL::build_dock_desc(*look, BuildingState::Normal, variant),
                    BuildingState::Normal)
              .reach_x,
          Lot::k_half + 0.02F)
          << look->name_prefix << variant;
    }
  }
}

TEST(MasonryWallStyle, TurnsAWallKeyIntoTheStoneFamilyAndLeavesTheRestAlone) {
  using Game::Systems::NationID;
  EXPECT_TRUE(Game::Visuals::is_building_visual_style("wall_segment", "masonry"));
  EXPECT_TRUE(Game::Visuals::is_building_visual_style("wall_gate", "masonry"));

  EXPECT_EQ(Game::Visuals::apply_wall_style("troops/roman/wall_straight", "masonry"),
            "troops/roman_masonry/wall_straight");
  EXPECT_EQ(Game::Visuals::apply_wall_style("troops/carthage/wall_gate", "masonry"),
            "troops/carthage_masonry/wall_gate");
  EXPECT_EQ(Game::Visuals::apply_wall_style("troops/roman/wall_straight", ""),
            "troops/roman/wall_straight");
  EXPECT_EQ(Game::Visuals::apply_wall_style("troops/roman/wall_straight", "tenement"),
            "troops/roman/wall_straight");
  EXPECT_EQ(Game::Visuals::apply_wall_style("troops/iberians/wall_straight", "masonry"),
            "troops/iberians/wall_straight")
      << "a nation without the style keeps its own wall";

  EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                NationID::RomanRepublic, "wall_gate", "masonry"),
            "troops/roman_masonry/wall_gate");
}
