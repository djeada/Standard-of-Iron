#include <algorithm>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "game/systems/nation_loader.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_catalog_loader.h"

namespace {

template <typename T, typename = void>
struct HasSelectionRingYOffsetMember : std::false_type {};

template <typename T>
struct HasSelectionRingYOffsetMember<
    T,
    std::void_t<decltype(std::declval<T>().selection_ring_y_offset)>> : std::true_type {
};

static_assert(!HasSelectionRingYOffsetMember<Game::Systems::NationTroopVariant>::value);
static_assert(!HasSelectionRingYOffsetMember<Game::Units::TroopVisualStats>::value);

TEST(NationLoader, ArcherProfilesKeepDefaultSelectionRingGroundOffset) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto& nation : nations) {
    registry.register_nation(nation);
  }

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                          Game::Units::TroopType::Archer);
  auto const carthage = profiles.get_profile(Game::Systems::NationID::Carthage,
                                             Game::Units::TroopType::Archer);

  EXPECT_FLOAT_EQ(roman.visuals.selection_ring_ground_offset, 0.0F);
  EXPECT_FLOAT_EQ(carthage.visuals.selection_ring_ground_offset, 0.0F);
}

TEST(NationLoader, ArcherProfilesReceiveRangeMultiplierAcrossNations) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto& nation : nations) {
    registry.register_nation(nation);
  }

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman_archer = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                                 Game::Units::TroopType::Archer);
  auto const roman_horse_archer = profiles.get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::HorseArcher);
  auto const carthage_archer = profiles.get_profile(Game::Systems::NationID::Carthage,
                                                    Game::Units::TroopType::Archer);
  auto const carthage_horse_archer = profiles.get_profile(
      Game::Systems::NationID::Carthage, Game::Units::TroopType::HorseArcher);

  EXPECT_FLOAT_EQ(roman_archer.combat.ranged_range, 11.4F);
  EXPECT_FLOAT_EQ(roman_horse_archer.combat.ranged_range, 12.9F);
  EXPECT_FLOAT_EQ(carthage_archer.combat.ranged_range, 10.8F);
  EXPECT_FLOAT_EQ(carthage_horse_archer.combat.ranged_range, 12.0F);
}

TEST(NationLoader, FrontlineProfilesKeepCatalogFormationSpacing) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto& nation : nations) {
    registry.register_nation(nation);
  }

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                          Game::Units::TroopType::Swordsman);
  auto const carthage = profiles.get_profile(Game::Systems::NationID::Carthage,
                                             Game::Units::TroopType::Spearman);

  EXPECT_FLOAT_EQ(roman.visuals.formation_spacing, 1.05F);
  EXPECT_FLOAT_EQ(carthage.visuals.formation_spacing, 1.05F);
}

TEST(NationLoader, CivilianProfilesUseNationSpecificRenderersWhenAvailable) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto& nation : nations) {
    registry.register_nation(nation);
  }

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                          Game::Units::TroopType::Civilian);
  auto const carthage = profiles.get_profile(Game::Systems::NationID::Carthage,
                                             Game::Units::TroopType::Civilian);

  EXPECT_EQ(roman.display_name, "Civilian");
  EXPECT_EQ(roman.visuals.renderer_id, "troops/roman/civilian");
  EXPECT_EQ(carthage.display_name, "Civilian");
  EXPECT_EQ(carthage.visuals.renderer_id, "troops/carthage/civilian");
}

TEST(NationLoader, ArcherProfilesReceiveRangeMultiplierWithoutNationData) {
  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();

  Game::Units::TroopCatalog::instance().reset_to_defaults();

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const archer = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                           Game::Units::TroopType::Archer);
  auto const horse_archer = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                                 Game::Units::TroopType::HorseArcher);

  EXPECT_FLOAT_EQ(archer.combat.ranged_range, 9.0F);
  EXPECT_FLOAT_EQ(horse_archer.combat.ranged_range, 10.5F);
}

TEST(NationLoader, CommanderProfilesKeepFallbackRenderersWithoutNationData) {
  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();

  Game::Units::TroopCatalog::instance().reset_to_defaults();

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const fabius =
      profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                           Game::Units::TroopType::RomanLegionOrganizer);
  auto const scipio = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                           Game::Units::TroopType::RomanVeteranConsul);
  auto const marcellus =
      profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                           Game::Units::TroopType::RomanFieldCommander);
  auto const hanno =
      profiles.get_profile(Game::Systems::NationID::Carthage,
                           Game::Units::TroopType::CarthageMercenaryBroker);
  auto const hasdrubal = profiles.get_profile(
      Game::Systems::NationID::Carthage, Game::Units::TroopType::CarthageCavalryPatron);
  auto const hannibal =
      profiles.get_profile(Game::Systems::NationID::Carthage,
                           Game::Units::TroopType::CarthageElephantMaster);

  EXPECT_EQ(fabius.visuals.renderer_id, "troops/roman/commanders/fabius_maximus");
  EXPECT_EQ(scipio.visuals.renderer_id, "troops/roman/commanders/scipio_africanus");
  EXPECT_EQ(marcellus.visuals.renderer_id, "troops/roman/commanders/marcellus");
  EXPECT_EQ(hanno.visuals.renderer_id, "troops/carthage/commanders/hanno_the_great");
  EXPECT_EQ(hasdrubal.visuals.renderer_id,
            "troops/carthage/commanders/hasdrubal_barca");
  EXPECT_EQ(hannibal.visuals.renderer_id, "troops/carthage/commanders/hannibal_barca");
}

TEST(NationLoader, ProfilesPreserveConfiguredResourceCostsAcrossNations) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto& nation : nations) {
    registry.register_nation(nation);
  }

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const roman_archer = profiles.get_profile(Game::Systems::NationID::RomanRepublic,
                                                 Game::Units::TroopType::Archer);
  auto const carthage_archer = profiles.get_profile(Game::Systems::NationID::Carthage,
                                                    Game::Units::TroopType::Archer);
  auto const roman_builder = profiles.get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::Builder);

  EXPECT_EQ(
      roman_archer.production.resource_costs.get(Game::Systems::ResourceType::Wood), 6);
  EXPECT_EQ(
      carthage_archer.production.resource_costs.get(Game::Systems::ResourceType::Wood),
      6);
  EXPECT_TRUE(roman_builder.production.resource_costs.empty());
}

TEST(NationLoader, IronSepulcherRoundTripsNationId) {
  Game::Systems::NationID parsed{};
  EXPECT_TRUE(
      Game::Systems::try_parse_nation_id(QStringLiteral("iron_sepulcher"), parsed));
  EXPECT_EQ(parsed, Game::Systems::NationID::IronSepulcher);
  EXPECT_EQ(Game::Systems::nation_id_to_qstring(Game::Systems::NationID::IronSepulcher),
            QStringLiteral("iron_sepulcher"));
}

TEST(NationLoader, IronSepulcherMetadataLoadsAsNonPlayableRoster) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto const it = std::find_if(
      nations.begin(), nations.end(), [](const Game::Systems::Nation& nation) {
        return nation.id == Game::Systems::NationID::IronSepulcher;
      });
  ASSERT_NE(it, nations.end());

  EXPECT_EQ(it->display_name, "The Iron Sepulcher");
  EXPECT_EQ(it->formation_type, Game::Systems::FormationType::Barbarian);
  EXPECT_FALSE(it->primary_building.has_value());
  EXPECT_FALSE(it->playable);
  EXPECT_FALSE(it->has_economy);
  EXPECT_EQ(it->ai_profile, "sepulcher_defense");
  EXPECT_FALSE(it->selectable_in_skirmish);
  ASSERT_EQ(it->available_troops.size(), 3U);
  EXPECT_NE(it->get_troop(Game::Units::TroopType::SkeletonSwordsman), nullptr);
  EXPECT_NE(it->get_troop(Game::Units::TroopType::SkeletonArcher), nullptr);
  EXPECT_NE(it->get_troop(Game::Units::TroopType::GravePriest), nullptr);
  ASSERT_TRUE(it->troop_variants.contains(Game::Units::TroopType::SkeletonArcher));
  ASSERT_TRUE(it->troop_variants.contains(Game::Units::TroopType::GravePriest));
  EXPECT_EQ(it->troop_variants.at(Game::Units::TroopType::SkeletonArcher).abilities,
            (std::vector<std::string>{"cursed_arrow_volley"}));
  EXPECT_EQ(it->troop_variants.at(Game::Units::TroopType::GravePriest).abilities,
            (std::vector<std::string>{"fireball"}));
}

TEST(NationLoader, IronSepulcherProfilesResolveUndeadRenderers) {
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  ASSERT_FALSE(nations.empty());

  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  for (const auto& nation : nations) {
    registry.register_nation(nation);
  }

  auto& profiles = Game::Systems::TroopProfileService::instance();
  profiles.clear();

  auto const skeleton_swordsman =
      profiles.get_profile(Game::Systems::NationID::IronSepulcher,
                           Game::Units::TroopType::SkeletonSwordsman);
  auto const skeleton_archer = profiles.get_profile(
      Game::Systems::NationID::IronSepulcher, Game::Units::TroopType::SkeletonArcher);
  auto const grave_priest = profiles.get_profile(Game::Systems::NationID::IronSepulcher,
                                                 Game::Units::TroopType::GravePriest);

  EXPECT_EQ(skeleton_swordsman.visuals.renderer_id,
            "troops/iron_sepulcher/skeleton_swordsman");
  EXPECT_EQ(skeleton_archer.visuals.renderer_id,
            "troops/iron_sepulcher/skeleton_archer");
  EXPECT_EQ(grave_priest.visuals.renderer_id, "troops/iron_sepulcher/grave_priest");
  EXPECT_TRUE(skeleton_archer.has_ability("cursed_arrow_volley"));
  EXPECT_TRUE(grave_priest.has_ability("fireball"));
}

TEST(NationLoader, DefaultNationRemainsRomanRepublic) {
  auto& registry = Game::Systems::NationRegistry::instance();
  registry.clear();
  registry.clear_player_assignments();
  registry.initialize_defaults();

  EXPECT_EQ(registry.default_nation_id(), Game::Systems::NationID::RomanRepublic);
}

} // namespace
