#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "formation/unit_layout.h"
#include "formation/unit_layout_resolver.h"

namespace {

using Game::Formation::k_invalid_layout;
using Game::Formation::SoldierOffset;
using Game::Formation::UnitLayoutId;
using Game::Formation::UnitLayoutLibrary;
using Game::Formation::UnitLayoutQuery;
using Game::Formation::UnitLayoutState;
using Game::Formation::UnitLayoutSystem;

auto layout(const char* doctrine, const char* generic) -> UnitLayoutId {
  return UnitLayoutLibrary::instance().resolve(doctrine, generic);
}

auto block(UnitLayoutId id,
           int count,
           int max_per_row,
           float spacing = 1.0F,
           std::uint32_t seed = 0x1234ABCDU) -> std::vector<SoldierOffset> {
  return UnitLayoutSystem::instance().compute(id, count, max_per_row, spacing, seed);
}

auto lateral_span(const std::vector<SoldierOffset>& offsets) -> float {
  if (offsets.empty()) {
    return 0.0F;
  }
  auto const [min_it, max_it] = std::minmax_element(
      offsets.begin(), offsets.end(), [](const auto& a, const auto& b) {
        return a.offset_x < b.offset_x;
      });
  return max_it->offset_x - min_it->offset_x;
}

auto depth_span(const std::vector<SoldierOffset>& offsets) -> float {
  if (offsets.empty()) {
    return 0.0F;
  }
  auto const [min_it, max_it] = std::minmax_element(
      offsets.begin(), offsets.end(), [](const auto& a, const auto& b) {
        return a.offset_z < b.offset_z;
      });
  return max_it->offset_z - min_it->offset_z;
}

auto closest_pair_distance(const std::vector<SoldierOffset>& offsets) -> float {
  float best = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    for (std::size_t j = i + 1; j < offsets.size(); ++j) {
      float const dx = offsets[i].offset_x - offsets[j].offset_x;
      float const dz = offsets[i].offset_z - offsets[j].offset_z;
      best = std::min(best, std::sqrt(dx * dx + dz * dz));
    }
  }
  return offsets.size() < 2 ? 0.0F : best;
}

auto aspect_ratio(const std::vector<SoldierOffset>& offsets) -> float {
  float const depth = depth_span(offsets);
  return depth < 1.0e-3F ? 0.0F : lateral_span(offsets) / depth;
}

auto rank_curvature(const std::vector<SoldierOffset>& offsets,
                    int cols,
                    int rank) -> float {
  float sum = 0.0F;
  int count = 0;
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    if (static_cast<int>(i) / cols != rank) {
      continue;
    }
    sum += offsets[i].offset_z;
    min_z = std::min(min_z, offsets[i].offset_z);
    max_z = std::max(max_z, offsets[i].offset_z);
    ++count;
  }
  return count < 2 ? 0.0F : max_z - min_z;
}

auto rank_skew(const std::vector<SoldierOffset>& offsets, int cols) -> float {
  auto rank_centre = [&](int rank) {
    float sum = 0.0F;
    int count = 0;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
      if (static_cast<int>(i) / cols == rank) {
        sum += offsets[i].offset_x;
        ++count;
      }
    }
    return count == 0 ? 0.0F : sum / static_cast<float>(count);
  };
  int const last_rank = (static_cast<int>(offsets.size()) - 1) / cols;
  return std::abs(rank_centre(last_rank) - rank_centre(0));
}

auto rank_pitch(const std::vector<SoldierOffset>& offsets, int cols) -> float {
  auto rank_centre = [&](int rank) {
    float sum = 0.0F;
    int count = 0;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
      if (static_cast<int>(i) / cols == rank) {
        sum += offsets[i].offset_z;
        ++count;
      }
    }
    return count == 0 ? 0.0F : sum / static_cast<float>(count);
  };
  int const last_rank = (static_cast<int>(offsets.size()) - 1) / cols;
  if (last_rank < 1) {
    return 0.0F;
  }
  return std::abs(rank_centre(last_rank) - rank_centre(0)) /
         static_cast<float>(last_rank);
}

auto mean_absolute_yaw(const std::vector<SoldierOffset>& offsets) -> float {
  if (offsets.empty()) {
    return 0.0F;
  }
  float sum = 0.0F;
  for (const auto& offset : offsets) {
    sum += std::abs(offset.yaw_offset);
  }
  return sum / static_cast<float>(offsets.size());
}

class UnitLayoutTest : public ::testing::Test {
protected:
  void SetUp() override { UnitLayoutLibrary::instance().reset_to_defaults(); }
};

} // namespace

TEST_F(UnitLayoutTest, LibraryResolvesDoctrineVariantsBeforeGenericStyles) {
  auto const generic = layout("", "close_order_infantry");
  auto const roman = layout("rome", "close_order_infantry");
  auto const carthaginian = layout("carthage", "close_order_infantry");
  auto const sepulcher = layout("iron_sepulcher", "close_order_infantry");

  EXPECT_NE(generic, k_invalid_layout);
  EXPECT_NE(roman, generic);
  EXPECT_NE(carthaginian, roman);
  EXPECT_NE(sepulcher, roman);
  EXPECT_NE(sepulcher, carthaginian);
}

TEST_F(UnitLayoutTest, UnknownDoctrineFallsBackToGenericStyle) {
  EXPECT_EQ(layout("atlantis", "close_order_infantry"),
            layout("", "close_order_infantry"));
}

TEST_F(UnitLayoutTest, OffsetsAreDeterministicForTheSameQuery) {
  auto const id = layout("carthage", "close_order_infantry");
  auto const first = block(id, 24, 6);
  auto const second = block(id, 24, 6);

  ASSERT_EQ(first.size(), second.size());
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i].offset_x, second[i].offset_x) << "index " << i;
    EXPECT_FLOAT_EQ(first[i].offset_z, second[i].offset_z) << "index " << i;
    EXPECT_FLOAT_EQ(first[i].yaw_offset, second[i].yaw_offset) << "index " << i;
  }
}

TEST_F(UnitLayoutTest, DifferentSeedsProduceDifferentVariationButSameFootprint) {
  auto const id = layout("carthage", "close_order_infantry");
  auto const a = block(id, 24, 6, 1.0F, 1U);
  auto const b = block(id, 24, 6, 1.0F, 2U);

  bool any_difference = false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::abs(a[i].offset_x - b[i].offset_x) > 1.0e-4F) {
      any_difference = true;
      break;
    }
  }
  EXPECT_TRUE(any_difference);
  EXPECT_NEAR(lateral_span(a), lateral_span(b), 0.5F);
}

TEST_F(UnitLayoutTest, RomanRanksAreTighterAndStraighterThanCarthaginian) {
  auto const roman = block(layout("rome", "close_order_infantry"), 24, 6);
  auto const carthaginian = block(layout("carthage", "close_order_infantry"), 24, 6);

  EXPECT_LT(lateral_span(roman), lateral_span(carthaginian));
  EXPECT_LT(mean_absolute_yaw(roman), mean_absolute_yaw(carthaginian));
}

TEST_F(UnitLayoutTest, RomeIsADeepNarrowBlockAndCarthageAWideShallowOne) {
  auto const roman = block(layout("rome", "close_order_infantry"), 24, 6);
  auto const carthaginian = block(layout("carthage", "close_order_infantry"), 24, 6);

  EXPECT_GT(depth_span(roman), depth_span(carthaginian));
  EXPECT_GT(lateral_span(carthaginian), lateral_span(roman) * 1.5F);
  EXPECT_GT(aspect_ratio(carthaginian), aspect_ratio(roman) * 2.0F);
}

TEST_F(UnitLayoutTest, RomanRanksAreStraightWhileCarthaginianRanksBow) {
  auto const roman = block(layout("rome", "close_order_infantry"), 24, 6);
  auto const carthaginian = block(layout("carthage", "close_order_infantry"), 24, 6);

  EXPECT_LT(rank_curvature(roman, 6, 0), 0.15F);
  EXPECT_GT(rank_curvature(carthaginian, 6, 0), rank_curvature(roman, 6, 0) * 3.0F);
}

TEST_F(UnitLayoutTest, RomanRanksStackSquareWhileCarthaginianRanksEchelon) {
  auto const roman = block(layout("rome", "close_order_infantry"), 24, 6);
  auto const carthaginian = block(layout("carthage", "close_order_infantry"), 24, 6);

  EXPECT_LT(rank_skew(roman, 6), 0.2F);
  EXPECT_GT(rank_skew(carthaginian, 6), 0.75F);
  EXPECT_GT(rank_skew(carthaginian, 6), rank_skew(roman, 6) * 5.0F);
}

TEST_F(UnitLayoutTest, EveryPairedRoleKeepsTheFactionSilhouetteApart) {
  const char* const paired[] = {
      "close_order_infantry", "spear_ranks", "loose_order_ranged", "cavalry_wedge"};

  for (const auto* generic : paired) {
    auto const roman = block(layout("rome", generic), 24, 6);
    auto const carthaginian = block(layout("carthage", generic), 24, 6);

    EXPECT_GT(lateral_span(carthaginian), lateral_span(roman) * 1.25F) << generic;
    EXPECT_GT(rank_pitch(roman, 6), rank_pitch(carthaginian, 6)) << generic;
    EXPECT_GT(mean_absolute_yaw(carthaginian), mean_absolute_yaw(roman) * 3.0F)
        << generic;
  }
}

TEST_F(UnitLayoutTest, TheThreeDoctrinesAreMutuallyDistinguishable) {
  auto const roman = block(layout("rome", "close_order_infantry"), 24, 6);
  auto const carthaginian = block(layout("carthage", "close_order_infantry"), 24, 6);
  auto const sepulcher = block(layout("iron_sepulcher", "close_order_infantry"), 24, 6);

  float const rome_aspect = aspect_ratio(roman);
  float const carthage_aspect = aspect_ratio(carthaginian);
  float const sepulcher_aspect = aspect_ratio(sepulcher);

  EXPECT_GT(std::abs(carthage_aspect - rome_aspect), 0.8F);
  EXPECT_GT(std::abs(carthage_aspect - sepulcher_aspect), 0.8F);
  EXPECT_GT(std::abs(rome_aspect - sepulcher_aspect), 0.25F);

  EXPECT_LT(lateral_span(sepulcher), lateral_span(roman));
  EXPECT_LT(depth_span(sepulcher), depth_span(roman));
}

TEST_F(UnitLayoutTest, IronSepulcherIsTheDensestOfTheThreeDoctrines) {
  auto const roman = block(layout("rome", "close_order_infantry"), 24, 6);
  auto const carthaginian = block(layout("carthage", "close_order_infantry"), 24, 6);
  auto const sepulcher = block(layout("iron_sepulcher", "close_order_infantry"), 24, 6);

  EXPECT_LT(lateral_span(sepulcher), lateral_span(roman));
  EXPECT_LT(lateral_span(sepulcher), lateral_span(carthaginian));
  EXPECT_LT(depth_span(sepulcher), depth_span(roman));
}

TEST_F(UnitLayoutTest, SoldiersNeverOverlapExcessively) {
  const char* const doctrines[] = {"rome", "carthage", "iron_sepulcher", ""};
  const char* const styles[] = {"close_order_infantry",
                                "spear_ranks",
                                "loose_order_ranged",
                                "cavalry_wedge",
                                "burial_guard_block"};

  for (const auto* doctrine : doctrines) {
    for (const auto* style : styles) {
      auto const offsets = block(layout(doctrine, style), 20, 5, 1.0F);
      EXPECT_GT(closest_pair_distance(offsets), 0.30F) << doctrine << "/" << style;
    }
  }
}

TEST_F(UnitLayoutTest, SmallAndLargeCountsBothProduceValidLayouts) {
  auto const id = layout("rome", "close_order_infantry");
  for (int count : {1, 2, 3, 5, 7, 12, 40, 96}) {
    auto const offsets = block(id, count, 6);
    ASSERT_EQ(static_cast<int>(offsets.size()), count) << "count " << count;
    for (const auto& offset : offsets) {
      EXPECT_TRUE(std::isfinite(offset.offset_x));
      EXPECT_TRUE(std::isfinite(offset.offset_z));
      EXPECT_TRUE(std::isfinite(offset.yaw_offset));
    }
    if (count > 1) {
      EXPECT_GT(closest_pair_distance(offsets), 0.25F) << "count " << count;
    }
  }
}

TEST_F(UnitLayoutTest, SingleSoldierSitsAtTheUnitOrigin) {
  auto const offsets = block(layout("rome", "close_order_infantry"), 1, 6);
  ASSERT_EQ(offsets.size(), 1U);
  EXPECT_NEAR(offsets.front().offset_x, 0.0F, 1.0e-4F);
  EXPECT_NEAR(offsets.front().offset_z, 0.0F, 1.0e-4F);
}

TEST_F(UnitLayoutTest, SpearRanksAreDeeperThanCloseOrderInfantry) {
  auto const infantry = block(layout("rome", "close_order_infantry"), 20, 5);
  auto const spears = block(layout("rome", "spear_ranks"), 20, 5);
  EXPECT_GT(depth_span(spears), depth_span(infantry));
}

TEST_F(UnitLayoutTest, ArchersUseLooserSpacingThanLineInfantry) {
  auto const infantry = block(layout("carthage", "close_order_infantry"), 20, 5);
  auto const archers = block(layout("carthage", "loose_order_ranged"), 20, 5);
  EXPECT_GT(lateral_span(archers), lateral_span(infantry));
  EXPECT_GT(mean_absolute_yaw(archers), mean_absolute_yaw(infantry));
}

TEST_F(UnitLayoutTest, CavalryUsesMoreDepthPerRankThanInfantry) {
  auto const infantry = block(layout("rome", "close_order_infantry"), 12, 4);
  auto const cavalry = block(layout("rome", "cavalry_wedge"), 12, 4);
  EXPECT_GT(depth_span(cavalry), depth_span(infantry));
}

TEST_F(UnitLayoutTest, WorkPartiesFaceInwardAroundTheirSite) {
  auto const id = layout("rome", "work_party");
  constexpr int k_total = 8;
  for (int index = 0; index < k_total; ++index) {
    UnitLayoutQuery query;
    query.layout = id;
    query.index = index;
    query.row = 0;
    query.col = index;
    query.rows = 1;
    query.cols = k_total;
    query.spacing = 2.0F;
    query.seed = 0U;
    auto const offset = UnitLayoutSystem::instance().offset(query);

    float const radius = std::hypot(offset.offset_x, offset.offset_z);
    ASSERT_GT(radius, 0.5F) << "index " << index;

    float const yaw_rad = offset.yaw_offset * (std::numbers::pi_v<float> / 180.0F);
    float const facing_x = std::sin(yaw_rad);
    float const facing_z = std::cos(yaw_rad);
    EXPECT_NEAR(facing_x, -offset.offset_x / radius, 0.05F) << "index " << index;
    EXPECT_NEAR(facing_z, -offset.offset_z / radius, 0.05F) << "index " << index;
  }
}

TEST_F(UnitLayoutTest, DefensiveStateTightensAndSteadiesTheBlock) {
  auto const base =
      UnitLayoutLibrary::instance().style(layout("rome", "close_order_infantry"));
  auto const defensive =
      Game::Formation::apply_state_modifier(base, UnitLayoutState::Defensive);

  EXPECT_LT(defensive.lateral_spacing_scale, base.lateral_spacing_scale);
  EXPECT_LT(defensive.lateral_jitter, base.lateral_jitter);
  EXPECT_LT(defensive.facing_jitter_degrees, base.facing_jitter_degrees);
}

TEST_F(UnitLayoutTest, BracedStateAddsWeaponClearanceDepth) {
  auto const base = UnitLayoutLibrary::instance().style(layout("rome", "spear_ranks"));
  auto const braced =
      Game::Formation::apply_state_modifier(base, UnitLayoutState::Braced);

  EXPECT_GT(braced.depth_spacing_scale, base.depth_spacing_scale);
  EXPECT_GE(braced.weapon_clearance, base.weapon_clearance);
}

TEST_F(UnitLayoutTest, RoutingStateScattersTheUnit) {
  auto const base =
      UnitLayoutLibrary::instance().style(layout("rome", "close_order_infantry"));
  auto const routing =
      Game::Formation::apply_state_modifier(base, UnitLayoutState::Routing);

  EXPECT_GT(routing.lateral_jitter, base.lateral_jitter);
  EXPECT_GT(routing.facing_jitter_degrees, base.facing_jitter_degrees);
}

TEST_F(UnitLayoutTest, PartiallyFormedUnitsAreLooserThanFormedOnes) {
  auto const id = layout("rome", "close_order_infantry");
  auto const formed = UnitLayoutSystem::instance().compute(id, 20, 5, 1.0F, 7U, 1.0F);
  auto const forming = UnitLayoutSystem::instance().compute(id, 20, 5, 1.0F, 7U, 0.2F);

  EXPECT_GT(lateral_span(forming), lateral_span(formed));
}

TEST_F(UnitLayoutTest, LayoutSelectionFollowsTroopRoleAndState) {
  using Game::Units::TroopType;

  auto const normal = Game::Formation::select_unit_layout(
      "rome", TroopType::Swordsman, UnitLayoutState::Normal);
  auto const defensive = Game::Formation::select_unit_layout(
      "rome", TroopType::Swordsman, UnitLayoutState::Defensive);
  auto const marching = Game::Formation::select_unit_layout(
      "rome", TroopType::Swordsman, UnitLayoutState::Marching);

  EXPECT_NE(normal, defensive);
  EXPECT_NE(normal, marching);
  EXPECT_EQ(defensive, layout("rome", "shield_wall"));
  EXPECT_EQ(marching, layout("rome", "marching_column"));
}

TEST_F(UnitLayoutTest, ConstructingBuildersOverrideTheirTroopLayout) {
  using Game::Units::TroopType;
  auto const working = Game::Formation::select_unit_layout(
      "rome", TroopType::Builder, UnitLayoutState::Normal, true);
  EXPECT_EQ(working, layout("rome", "work_party"));
}

TEST_F(UnitLayoutTest, EveryTroopTypeResolvesToARegisteredLayout) {
  using Game::Units::TroopType;
  for (int i = 0; i <= static_cast<int>(TroopType::Builder); ++i) {
    auto const troop = static_cast<TroopType>(i);
    for (const auto* doctrine : {"rome", "carthage", "iron_sepulcher", "neutral"}) {
      auto const id =
          Game::Formation::select_unit_layout(doctrine, troop, UnitLayoutState::Normal);
      EXPECT_NE(id, k_invalid_layout) << "troop " << i << " doctrine " << doctrine;
    }
  }
}
