#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <gtest/gtest.h>

#include "app/economy/production_readouts.h"
#include "app/economy/unit_profile.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/troop_catalog_loader.h"
#include "game/units/troop_type.h"

namespace {

class UnitProfileTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Units::TroopCatalogLoader::load_default_catalog();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    Game::Systems::TroopProfileService::instance().clear();
  }
};

TEST_F(UnitProfileTest, TheAdvertisedPriceIsWhatProductionCharges) {
  auto& profiles = Game::Systems::TroopProfileService::instance();
  for (const auto* nation : {"carthage", "roman_republic"}) {
    const auto nation_id = Game::Systems::nation_id_from_string(nation);
    ASSERT_TRUE(nation_id.has_value());
    for (const auto* unit : {"archer", "swordsman", "spearman", "builder"}) {
      const auto profile = App::Economy::unit_profile(
          Game::Systems::NationRegistry::instance(), unit, nation);
      if (!profile.value("valid").toBool()) {
        continue;
      }
      const auto troop_type = Game::Units::try_parse_troop_type(unit);
      ASSERT_TRUE(troop_type.has_value());
      EXPECT_EQ(profile.value("cost").toInt(),
                profiles.get_profile(*nation_id, *troop_type).production.cost)
          << nation << " " << unit
          << ": the card must quote the price the barracks spends from its reserve";
      EXPECT_FALSE(profile.contains("population_cost"))
          << nation << " " << unit
          << ": a second price key is what let the card and the barracks drift apart";
    }
  }
}

TEST_F(UnitProfileTest, ARecruitableUnitCarriesStatsCostsRolesAndLore) {
  const auto profile = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "spearman", "roman_republic");

  ASSERT_TRUE(profile.value("valid").toBool());
  EXPECT_EQ(profile.value("display_name").toString(), QStringLiteral("Triarius"));

  EXPECT_GT(profile.value("health").toInt(), 0);
  EXPECT_GT(profile.value("attack_damage").toInt(), 0);
  EXPECT_GT(profile.value("attack_range").toDouble(), 0.0);
  EXPECT_GT(profile.value("speed").toDouble(), 0.0);
  EXPECT_GT(profile.value("vision_range").toDouble(), 0.0);
  EXPECT_GT(profile.value("damage_per_second").toDouble(), 0.0);
  EXPECT_GT(profile.value("build_time").toDouble(), 0.0);

  const auto roles = profile.value("role_tags").toStringList();
  EXPECT_FALSE(roles.isEmpty())
      << "the spearman declares formation roles, so the panel must have chips to draw";

  EXPECT_TRUE(profile.value("has_lore").toBool());
  EXPECT_FALSE(profile.value("role").toString().isEmpty());
  EXPECT_FALSE(profile.value("strengths").toString().isEmpty());
  EXPECT_FALSE(profile.value("weaknesses").toString().isEmpty());
  EXPECT_FALSE(profile.value("history").toString().isEmpty());
}

TEST_F(UnitProfileTest, NationsOverrideTheHistoryThatFollowsTheirNameForTheUnit) {
  const auto roman = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "swordsman", "roman_republic");
  const auto carthaginian = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "swordsman", "carthage");

  ASSERT_TRUE(roman.value("valid").toBool());
  ASSERT_TRUE(carthaginian.value("valid").toBool());

  EXPECT_NE(roman.value("display_name").toString(),
            carthaginian.value("display_name").toString());
  EXPECT_NE(roman.value("history").toString(), carthaginian.value("history").toString())
      << "a Legionary and Citizen Infantry share stats but not a past";

  EXPECT_EQ(roman.value("strengths").toString(),
            carthaginian.value("strengths").toString())
      << "strengths follow the stats, which the nations share, so they must not fork";
}

TEST_F(UnitProfileTest, ARangedUnitReportsItsRangedAttackAsThePrimaryOne) {
  const auto archer = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "archer", "roman_republic");
  ASSERT_TRUE(archer.value("valid").toBool());

  EXPECT_TRUE(archer.value("prefers_ranged").toBool());
  EXPECT_EQ(archer.value("attack_damage").toInt(),
            archer.value("ranged_damage").toInt());
  EXPECT_DOUBLE_EQ(archer.value("attack_range").toDouble(),
                   archer.value("ranged_range").toDouble());

  const auto swordsman = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "swordsman", "roman_republic");
  ASSERT_TRUE(swordsman.value("valid").toBool());
  EXPECT_FALSE(swordsman.value("prefers_ranged").toBool());
  EXPECT_EQ(swordsman.value("attack_damage").toInt(),
            swordsman.value("melee_damage").toInt());
}

TEST_F(UnitProfileTest, ADocumentedAbilityCarriesItsNameAndEffect) {
  const auto priest = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "grave_priest", "iron_sepulcher");
  ASSERT_TRUE(priest.value("valid").toBool());

  const auto abilities = priest.value("abilities").toList();
  ASSERT_EQ(abilities.size(), 1);
  const auto entry = abilities.front().toMap();
  EXPECT_EQ(entry.value("id").toString(), QStringLiteral("fireball"));
  EXPECT_EQ(entry.value("name").toString(), QStringLiteral("Fireball"));
  EXPECT_FALSE(entry.value("effect").toString().isEmpty())
      << "an ability with no effect line tells the player nothing";
}

TEST_F(UnitProfileTest, ACommanderKeepsItsAuthoredProse) {
  const auto commander =
      App::Economy::unit_profile(Game::Systems::NationRegistry::instance(),
                                 "carthage_sword_commander",
                                 "carthage");
  ASSERT_TRUE(commander.value("valid").toBool());

  EXPECT_TRUE(commander.value("is_commander").toBool());
  EXPECT_TRUE(commander.value("has_lore").toBool());
  EXPECT_FALSE(commander.value("strengths").toString().isEmpty());
  EXPECT_FALSE(commander.value("weaknesses").toString().isEmpty());
  EXPECT_FALSE(commander.value("passive_aura").toString().isEmpty());
  EXPECT_FALSE(commander.value("rally_ability").toString().isEmpty());
}

TEST_F(UnitProfileTest, AnUnknownUnitIsReportedInvalidRatherThanFabricated) {
  const auto profile = App::Economy::unit_profile(
      Game::Systems::NationRegistry::instance(), "not_a_unit", "roman_republic");

  EXPECT_FALSE(profile.value("valid").toBool());
  EXPECT_FALSE(profile.value("has_lore").toBool());
  EXPECT_TRUE(profile.value("role_tags").toStringList().isEmpty());
  EXPECT_TRUE(profile.value("abilities").toList().isEmpty());
}

TEST_F(UnitProfileTest, TheRecruitCardAndTheInspectPanelReadTheSameNumbers) {
  for (const auto* unit_type : {"archer",
                                "swordsman",
                                "spearman",
                                "horse_swordsman",
                                "horse_archer",
                                "horse_spearman",
                                "healer",
                                "builder",
                                "elephant"}) {
    const QString type = QString::fromLatin1(unit_type);
    const auto recruit = App::Economy::unit_production_info(
        Game::Systems::NationRegistry::instance(), type, "roman_republic");
    const auto inspect = App::Economy::unit_profile(
        Game::Systems::NationRegistry::instance(), type, "roman_republic");

    for (const auto* key : {"display_name",
                            "cost",
                            "build_time",
                            "individuals_per_unit",
                            "resource_costs",
                            "is_commander"}) {
      const QString shared_key = QString::fromLatin1(key);
      EXPECT_EQ(recruit.value(shared_key), inspect.value(shared_key))
          << unit_type << " disagrees on " << key
          << ": the recruit card and the inspect panel have drifted apart";
    }
  }
}

} // namespace
