#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <string>

#include "game/systems/building_collision_registry.h"
#include "game/units/building_spawn_setup.h"
#include "game/visuals/building_asset_key.h"
#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/carthage/tenement_authoring.h"
#include "render/entity/nations/roman/tenement_authoring.h"
#include "render/entity/tenement_layout.h"
#include "render/geom/mesh.h"

namespace {

using Render::GL::BuildingState;
using Render::GL::k_tenement_variant_count;
using Render::GL::tenement_traits;
using Render::GL::tenement_visual_variant;
namespace Lot = Render::GL::TenementLot;

constexpr std::array<BuildingState, 3> k_states{
    BuildingState::Normal, BuildingState::Damaged, BuildingState::Destroyed};

using TenementAuthor = Render::GL::BuildingArchetypeDesc (*)(BuildingState, int);

struct Nation {
  const char* name;
  TenementAuthor build;
};

constexpr std::array<Nation, 2> k_nations{
    {{"roman", &Render::GL::Roman::build_tenement_desc},
     {"carthage", &Render::GL::Carthage::build_tenement_desc}}};

// Every vertex a tenement variant draws, in building-local units.
template <typename Visit>
void for_each_vertex(const Nation& nation,
                     int variant,
                     BuildingState state,
                     Visit&& visit) {
  const auto archetype =
      Render::GL::build_building_archetype(nation.build(state, variant), state);
  for (const auto& draw :
       archetype.lods[static_cast<std::size_t>(Render::GL::RenderArchetypeLod::Full)]
           .draws) {
    if (draw.mesh == nullptr) {
      continue;
    }
    float low = 1.0e9F;
    float high = -1.0e9F;
    std::vector<QVector3D> points;
    points.reserve(draw.mesh->get_vertices().size());
    for (const auto& vertex : draw.mesh->get_vertices()) {
      points.push_back(draw.local_model.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2])));
      low = std::min(low, points.back().y());
      high = std::max(high, points.back().y());
    }
    for (const auto& point : points) {
      visit(point, low, high);
    }
  }
}

} // namespace

TEST(TenementStyle, IsAHomeStyleWithItsOwnRendererKey) {
  using Game::Systems::NationID;
  EXPECT_TRUE(Game::Visuals::is_building_visual_style("home", "tenement"));
  EXPECT_FALSE(Game::Visuals::is_building_visual_style("temple", "tenement"));
  EXPECT_FALSE(Game::Visuals::is_building_visual_style("home", ""));

  EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                NationID::RomanRepublic, "home", "tenement"),
            "troops/roman/home_tenement");
  EXPECT_EQ(
      Game::Visuals::styled_building_asset_key(NationID::RomanRepublic, "home", ""),
      "troops/roman/home");
  EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                NationID::RomanRepublic, "home", "no_such_style"),
            "troops/roman/home");
  EXPECT_EQ(
      Game::Visuals::styled_building_asset_key(NationID::Carthage, "home", "tenement"),
      "troops/carthage/home_tenement");
  EXPECT_EQ(Game::Visuals::styled_building_asset_key(
                NationID::IronSepulcher, "home", "tenement"),
            "troops/iron_sepulcher/home")
      << "a nation without the style draws its plain building";
}

TEST(TenementStyle, EveryStyleHasItsVariantsInTheBakeCatalog) {
  int found = 0;
  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    const bool roman = entry.name.starts_with("roman_tenement_variant_");
    const bool punic = entry.name.starts_with("carthage_tenement_variant_");
    if (!roman && !punic) {
      continue;
    }
    ++found;
    ASSERT_EQ(entry.kinds.size(), 1U);
    EXPECT_EQ(entry.kinds.front(),
              roman ? "troops/roman/home_tenement" : "troops/carthage/home_tenement");
    for (const BuildingState state : k_states) {
      EXPECT_EQ(entry.build(state).name(), entry.name);
      EXPECT_FALSE(entry.build(state).parts().empty()) << entry.name;
    }
  }
  EXPECT_EQ(found, 2 * k_tenement_variant_count);
}

TEST(TenementLot, PartyWallsMeetOnTheHomeNavigationFootprint) {
  const auto nav = Game::Systems::BuildingCollisionRegistry::get_building_size("home");
  const QVector3D scale = Game::Units::building_transform_scale("home");
  const float drawn_width = 2.0F * Lot::k_half_width * scale.x();
  EXPECT_LE(drawn_width, nav.width)
      << "a tenement wider than its lot would stand in its neighbour";
  EXPECT_GE(drawn_width, nav.width - 0.05F)
      << "two tenements on neighbouring lots must close to a party wall, not "
         "leave a slot of daylight between them";
  EXPECT_LE(-Lot::k_rear * scale.z(), nav.depth * 0.5F);
  EXPECT_LE(Lot::k_front * scale.z(), nav.depth * 0.5F);
}

TEST(TenementLot, NoVariantReachesIntoTheNeighbouringLot) {
  for (const Nation& nation : k_nations) {
    for (int variant = 0; variant < k_tenement_variant_count; ++variant) {
      for (const BuildingState state : k_states) {
        float reach = 0.0F;
        for_each_vertex(
            nation, variant, state, [&](const QVector3D& point, float, float) {
              reach = std::max(reach, std::abs(point.x()));
            });
        // Destroyed houses spill rubble; standing ones keep to their lot.
        const float allowed =
            state == BuildingState::Normal ? Lot::k_half_width + 0.02F : 1.45F;
        EXPECT_LE(reach, allowed) << nation.name << " variant " << variant << " state "
                                  << static_cast<int>(state);
      }
    }
  }
}

TEST(TenementLot, NothingAtWalkingHeightStandsOutsideTheFootprint) {
  // Awnings, balconies and signs hang over the pavement above head height, and
  // jars and steps sit below the knee. What a body could walk into has to stay
  // on the lot: nothing drawn may be a blocker the navigation grid cannot see.
  const auto nav = Game::Systems::BuildingCollisionRegistry::get_building_size("home");
  const QVector3D scale = Game::Units::building_transform_scale("home");
  const float half_depth = (nav.depth * 0.5F) / scale.z();
  constexpr float k_knee = 0.36F;
  constexpr float k_head = 0.62F;
  constexpr float k_trim = 0.05F;
  for (const Nation& nation : k_nations) {
    for (int variant = 0; variant < k_tenement_variant_count; ++variant) {
      float front = 0.0F;
      float rear = 0.0F;
      for_each_vertex(nation,
                      variant,
                      BuildingState::Normal,
                      [&](const QVector3D& point, float low, float high) {
                        if (high <= k_knee || low >= k_head) {
                          return;
                        }
                        front = std::max(front, point.z());
                        rear = std::min(rear, point.z());
                      });
      EXPECT_LE(front, half_depth + k_trim) << nation.name << " variant " << variant;
      EXPECT_GE(rear, -half_depth - k_trim) << nation.name << " variant " << variant;
    }
  }
}

TEST(TenementLot, StoreyCountSetsTheHeightOfTheHouse) {
  for (const Nation& nation : k_nations) {
    for (int variant = 0; variant < k_tenement_variant_count; ++variant) {
      const auto& traits = tenement_traits(variant);
      ASSERT_GE(traits.storeys, 2);
      ASSERT_LE(traits.storeys, 4);
      float top = 0.0F;
      for_each_vertex(nation,
                      variant,
                      BuildingState::Normal,
                      [&](const QVector3D& point, float, float) {
                        top = std::max(top, point.y());
                      });
      const float wall_top = Render::GL::tenement_wall_top(traits);
      EXPECT_GT(top, wall_top) << "variant " << variant << " has no roof";
      EXPECT_LT(top, wall_top + 0.9F) << "variant " << variant;

      float ruin_top = 0.0F;
      for_each_vertex(nation,
                      variant,
                      BuildingState::Destroyed,
                      [&](const QVector3D& point, float, float) {
                        ruin_top = std::max(ruin_top, point.y());
                      });
      EXPECT_LT(ruin_top, Lot::k_ground_storey + 0.3F)
          << "a destroyed " << nation.name << " variant " << variant
          << " still stands above its street storey";
    }
  }
}

TEST(TenementVariant, IsAFunctionOfThePositionAlone) {
  for (float x = -80.0F; x < 80.0F; x += 4.3F) {
    for (float z = -40.0F; z < 40.0F; z += 4.4F) {
      const int variant = tenement_visual_variant(x, z);
      ASSERT_GE(variant, 0);
      ASSERT_LT(variant, k_tenement_variant_count);
      EXPECT_EQ(variant, tenement_visual_variant(x, z));
    }
  }
}

TEST(TenementVariant, AStreetShowsEveryVariantAndGroupsItsHeights) {
  std::array<int, k_tenement_variant_count> used{};
  int pairs = 0;
  int level = 0;
  int cliffs = 0;
  for (float z = -66.0F; z <= 66.0F; z += 22.0F) {
    int previous = 0;
    for (int lot = -40; lot <= 40; ++lot) {
      const int variant = tenement_visual_variant(static_cast<float>(lot) * 4.3F, z);
      ++used[static_cast<std::size_t>(variant)];
      const int storeys = tenement_traits(variant).storeys;
      if (previous != 0) {
        ++pairs;
        level += storeys == previous ? 1 : 0;
        cliffs += std::abs(storeys - previous) > 1 ? 1 : 0;
      }
      previous = storeys;
    }
  }
  EXPECT_TRUE(std::all_of(used.begin(), used.end(), [](int n) { return n > 0; }))
      << "a town should show the whole variant set";
  EXPECT_GT(level * 2, pairs) << "most neighbours share a storey count: " << level
                              << " of " << pairs;
  EXPECT_LT(level * 10, pairs * 9)
      << "but a street is not one flat cornice: " << level << " of " << pairs;
  EXPECT_LT(cliffs * 5, pairs)
      << cliffs << " of " << pairs << " neighbours differ by two storeys or more";
}
