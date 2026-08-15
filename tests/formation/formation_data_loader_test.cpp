#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "formation/formation_data_loader.h"
#include "formation/formation_doctrine.h"
#include "formation/troop_role_registry.h"
#include "formation/unit_layout.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::DoctrineRegistry;
using Game::Formation::FormationContentReport;
using Game::Formation::FormationDataLoader;
using Game::Formation::RoleTag;
using Game::Formation::TroopFormationProfile;
using Game::Formation::UnitLayoutLibrary;

auto object_from(const char* json) -> QJsonObject {
  return QJsonDocument::fromJson(QByteArray(json)).object();
}

class FormationDataLoaderTest : public ::testing::Test {
protected:
  void SetUp() override { FormationDataLoader::reset_to_builtin_defaults(); }
  void TearDown() override { FormationDataLoader::reset_to_builtin_defaults(); }
};

} // namespace

TEST_F(FormationDataLoaderTest, BuiltInContentPassesValidation) {
  FormationContentReport report;
  EXPECT_TRUE(FormationDataLoader::validate(report));
  for (const auto& issue : report.issues) {
    EXPECT_FALSE(issue.fatal) << issue.file.toStdString() << ": "
                              << issue.message.toStdString();
  }
}

TEST_F(FormationDataLoaderTest, ShippedFormationContentLoadsWithoutErrors) {
  auto const report = FormationDataLoader::load_all();
  for (const auto& issue : report.issues) {
    EXPECT_FALSE(issue.fatal) << issue.file.toStdString() << ": "
                              << issue.message.toStdString();
  }
  EXPECT_FALSE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, LayoutFileOverlaysTheBuiltInStyle) {
  auto& library = UnitLayoutLibrary::instance();
  auto const id = library.find("rome.close_order_infantry");
  ASSERT_NE(id, Game::Formation::k_invalid_layout);
  float const original_depth = library.style(id).depth_spacing_scale;

  FormationContentReport report;
  ASSERT_TRUE(FormationDataLoader::load_layout(
      object_from(R"({"id": "rome.close_order_infantry",
                      "lateral_spacing_scale": 2.5})"),
      report,
      "test"));

  auto const updated = library.find("rome.close_order_infantry");
  EXPECT_EQ(updated, id);
  EXPECT_FLOAT_EQ(library.style(updated).lateral_spacing_scale, 2.5F);
  EXPECT_FLOAT_EQ(library.style(updated).depth_spacing_scale, original_depth);
}

TEST_F(FormationDataLoaderTest, LayoutFileCanIntroduceANewStyle) {
  FormationContentReport report;
  ASSERT_TRUE(FormationDataLoader::load_layout(
      object_from(R"({"id": "atlantis.phalanx", "shape": "wedge",
                      "lateral_spacing_scale": 1.4})"),
      report,
      "test"));

  auto const id = UnitLayoutLibrary::instance().find("atlantis.phalanx");
  ASSERT_NE(id, Game::Formation::k_invalid_layout);
  EXPECT_EQ(UnitLayoutLibrary::instance().style(id).shape,
            Game::Formation::UnitLayoutShape::Wedge);
  EXPECT_EQ(UnitLayoutLibrary::instance().resolve("atlantis", "phalanx"), id);
}

TEST_F(FormationDataLoaderTest, LayoutWithoutAnIdIsRejected) {
  FormationContentReport report;
  EXPECT_FALSE(FormationDataLoader::load_layout(
      object_from(R"({"shape": "ranks"})"), report, "test"));
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, NonPositiveSpacingIsRejected) {
  FormationContentReport report;
  EXPECT_FALSE(FormationDataLoader::load_layout(
      object_from(R"({"id": "broken.style", "lateral_spacing_scale": 0.0})"),
      report,
      "test"));
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, UnknownShapeIsReportedAsAnError) {
  FormationContentReport report;
  FormationDataLoader::load_layout(
      object_from(R"({"id": "odd.style", "shape": "hypercube"})"), report, "test");
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, DoctrineFileDefinesIntentTemplates) {
  FormationContentReport report;
  ASSERT_TRUE(FormationDataLoader::load_doctrine(object_from(R"({
        "id": "atlantis",
        "display_name": "Atlantis",
        "default_intent": "line",
        "intents": {
          "faction_default": {
            "frontage_scale": 1.4,
            "lines": [
              {"role": "centre", "match_any": ["line_infantry"], "max_per_row": 5},
              {"role": "reserve", "max_per_row": 4}
            ]
          }
        }
      })"),
                                                 report,
                                                 "test"));

  const auto* doctrine = DoctrineRegistry::instance().find("atlantis");
  ASSERT_NE(doctrine, nullptr);
  EXPECT_EQ(doctrine->display_name, "Atlantis");
  EXPECT_EQ(doctrine->default_intent, ArmyFormationIntent::Line);

  const auto* tmpl = doctrine->resolve_template(ArmyFormationIntent::FactionDefault);
  ASSERT_NE(tmpl, nullptr);
  EXPECT_FLOAT_EQ(tmpl->frontage_scale, 1.4F);
  ASSERT_EQ(tmpl->lines.size(), 2U);
  EXPECT_EQ(tmpl->lines.front().max_per_row, 5);
}

TEST_F(FormationDataLoaderTest, UnknownRoleTagInADoctrineIsFatal) {
  FormationContentReport report;
  FormationDataLoader::load_doctrine(object_from(R"({
        "id": "broken",
        "intents": {
          "faction_default": {
            "lines": [{"role": "centre", "match_any": ["wizardry"]}]
          }
        }
      })"),
                                     report,
                                     "test");
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, UnknownIntentNameIsFatal) {
  FormationContentReport report;
  FormationDataLoader::load_doctrine(object_from(R"({
        "id": "broken_intent",
        "intents": {"phalanx_charge": {"lines": [{"role": "centre"}]}}
      })"),
                                     report,
                                     "test");
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, AnIntentWithNoLinesIsFatal) {
  FormationContentReport report;
  FormationDataLoader::load_doctrine(
      object_from(R"({"id": "empty", "intents": {"line": {"lines": []}}})"),
      report,
      "test");
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, TroopProfileParsesRolesAndLayouts) {
  TroopFormationProfile profile;
  ASSERT_TRUE(Game::Formation::parse_troop_formation_profile(object_from(R"({
        "roles": ["line_infantry", "shielded"],
        "unit_layout": "close_order_infantry",
        "defensive_layout": "shield_wall",
        "army_roles": ["centre", "reserve"]
      })"),
                                                             profile));

  EXPECT_TRUE(Game::Formation::has_role(profile.roles, RoleTag::LineInfantry));
  EXPECT_TRUE(Game::Formation::has_role(profile.roles, RoleTag::Shielded));
  EXPECT_EQ(profile.unit_layout, "close_order_infantry");
  EXPECT_EQ(profile.defensive_layout, "shield_wall");
  ASSERT_EQ(profile.army_roles.size(), 2U);
  EXPECT_EQ(profile.army_roles.front(), Game::Formation::ArmyRole::Centre);
}

TEST_F(FormationDataLoaderTest, TroopProfileWithNoFormationKeysIsLeftAlone) {
  TroopFormationProfile profile;
  EXPECT_FALSE(Game::Formation::parse_troop_formation_profile(
      object_from(R"({"individuals_per_unit": 12})"), profile));
  EXPECT_EQ(profile.roles, 0U);
}

TEST_F(FormationDataLoaderTest, ValidationCatchesTroopsPointingAtMissingLayouts) {
  TroopFormationProfile profile;
  profile.roles = Game::Formation::to_mask(RoleTag::LineInfantry);
  profile.unit_layout = "no_such_layout";
  Game::Formation::TroopRoleRegistry::instance().set_profile(
      Game::Units::TroopType::Swordsman, profile);

  FormationContentReport report;
  EXPECT_FALSE(FormationDataLoader::validate(report));
  EXPECT_TRUE(report.has_errors());
}

TEST_F(FormationDataLoaderTest, ShippedLayoutDataKeepsTheFactionsDistinguishable) {
  auto const report = FormationDataLoader::load_all();
  ASSERT_FALSE(report.has_errors());

  auto& library = UnitLayoutLibrary::instance();
  auto const roman = library.resolve("rome", "close_order_infantry");
  auto const carthaginian = library.resolve("carthage", "close_order_infantry");
  ASSERT_NE(roman, Game::Formation::k_invalid_layout);
  ASSERT_NE(carthaginian, Game::Formation::k_invalid_layout);

  const auto& rome_style = library.style(roman);
  const auto& carthage_style = library.style(carthaginian);

  EXPECT_GT(carthage_style.file_grouping, 1.0F);
  EXPECT_GT(carthage_style.group_gap, 0.2F);
  EXPECT_FLOAT_EQ(rome_style.file_grouping, 0.0F);
  EXPECT_GE(rome_style.rank_stagger, 0.4F);
  EXPECT_FLOAT_EQ(rome_style.rank_echelon, 0.0F);
  EXPECT_FLOAT_EQ(carthage_style.rank_echelon, 0.0F);
  EXPECT_GT(carthage_style.rank_arc, 0.3F);
  EXPECT_FLOAT_EQ(rome_style.rank_arc, 0.0F);
  EXPECT_GT(carthage_style.facing_jitter_degrees,
            rome_style.facing_jitter_degrees * 5.0F);
}

TEST_F(FormationDataLoaderTest, EveryShippedDoctrineHasAFactionDefaultTemplate) {
  for (const auto& id : DoctrineRegistry::instance().ids()) {
    const auto& doctrine = DoctrineRegistry::instance().get_or_neutral(id);
    EXPECT_NE(doctrine.resolve_template(ArmyFormationIntent::FactionDefault), nullptr)
        << id;
  }
}

TEST_F(FormationDataLoaderTest, DoctrinesCoverTheIntentsTheUIOffers) {
  for (const char* id : {"rome", "carthage"}) {
    const auto& doctrine = DoctrineRegistry::instance().get_or_neutral(id);
    for (auto intent : {ArmyFormationIntent::Line,
                        ArmyFormationIntent::Column,
                        ArmyFormationIntent::Defensive,
                        ArmyFormationIntent::Assault,
                        ArmyFormationIntent::Encirclement,
                        ArmyFormationIntent::SiegeEscort}) {
      EXPECT_TRUE(doctrine.supports(intent))
          << id << " / " << Game::Formation::intent_to_string(intent);
    }
  }
}
