#include "game/systems/nation_loader.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace {

template <typename T, typename = void>
struct HasSelectionRingYOffsetMember : std::false_type {};

template <typename T>
struct HasSelectionRingYOffsetMember<
    T, std::void_t<decltype(std::declval<T>().selection_ring_y_offset)>>
    : std::true_type {};

static_assert(
    !HasSelectionRingYOffsetMember<Game::Systems::NationTroopVariant>::value);
static_assert(
    !HasSelectionRingYOffsetMember<Game::Units::TroopVisualStats>::value);

TEST(NationLoader, ArcherProfilesKeepDefaultSelectionRingGroundOffset) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto &registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto &nation : nations) {
    registry.register_nation(nation);
  }

  auto &profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman = profiles.get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::Archer);
  auto const carthage = profiles.get_profile(Game::Systems::NationID::Carthage,
                                             Game::Units::TroopType::Archer);

  EXPECT_FLOAT_EQ(roman.visuals.selection_ring_ground_offset, 0.0F);
  EXPECT_FLOAT_EQ(carthage.visuals.selection_ring_ground_offset, 0.0F);
}

TEST(NationLoader, FrontlineProfilesKeepCatalogFormationSpacing) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto &registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto &nation : nations) {
    registry.register_nation(nation);
  }

  auto &profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman =
      profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                           Game::Units::TroopType::Swordsman);
  auto const carthage = profiles.get_profile(Game::Systems::NationID::Carthage,
                                             Game::Units::TroopType::Spearman);

  EXPECT_FLOAT_EQ(roman.visuals.formation_spacing, 1.05F);
  EXPECT_FLOAT_EQ(carthage.visuals.formation_spacing, 1.05F);
}

TEST(NationLoader, CivilianProfilesUseNationSpecificRenderersWhenAvailable) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto &registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto &nation : nations) {
    registry.register_nation(nation);
  }

  auto &profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman = profiles.get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::Civilian);
  auto const carthage = profiles.get_profile(Game::Systems::NationID::Carthage,
                                             Game::Units::TroopType::Civilian);

  EXPECT_EQ(roman.display_name, "Civilian");
  EXPECT_EQ(roman.visuals.renderer_id, "troops/roman/civilian");
  EXPECT_EQ(carthage.display_name, "Civilian");
  EXPECT_EQ(carthage.visuals.renderer_id, "troops/carthage/civilian");
}

} // namespace
