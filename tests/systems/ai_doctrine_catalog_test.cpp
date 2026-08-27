

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <gtest/gtest.h>
#include <set>

#include "game/systems/ai_system/ai_attack_wave.h"
#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/ai_system/ai_doctrine_catalog.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/units/commander_catalog.h"

namespace {

using namespace Game::Systems::AI;

auto write_file(const QDir& dir, const QString& name, const QString& body) -> QString {
  const QString path = dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return {};
  }
  QTextStream stream(&file);
  stream << body;
  return path;
}

class AIDoctrineCatalogTest : public ::testing::Test {
protected:
  void TearDown() override { reset_ai_doctrine_catalog(); }

  QTemporaryDir m_dir;
};

TEST_F(AIDoctrineCatalogTest, ShippedDataLoadsAndCoversEveryCommander) {
  reset_ai_doctrine_catalog();
  ASSERT_TRUE(load_default_ai_doctrine_catalog())
      << "the shipped assets/data/ai files did not load";

  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto* doctrine = authored_doctrine(definition.id);
    ASSERT_NE(doctrine, nullptr)
        << definition.id << " has no entry in the shipped doctrine file";
    EXPECT_NE(doctrine->town_plan, nullptr) << definition.id << " names no town plan";
    EXPECT_FALSE(doctrine->wave.target_priority.empty()) << definition.id;
    EXPECT_GT(doctrine->wave.size, 0) << definition.id;
  }
}

TEST_F(AIDoctrineCatalogTest, ShippedTownPlansAreDistinctAndNonEmpty) {
  reset_ai_doctrine_catalog();
  ASSERT_TRUE(load_default_ai_doctrine_catalog());

  std::set<std::string> plan_ids;
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto* doctrine = authored_doctrine(definition.id);
    ASSERT_NE(doctrine, nullptr);
    ASSERT_NE(doctrine->town_plan, nullptr);
    EXPECT_FALSE(doctrine->town_plan->steps.empty()) << definition.id;
    plan_ids.insert(doctrine->town_plan->id);
  }
  EXPECT_GE(plan_ids.size(), 4U)
      << "the commanders share so few town plans that their towns will look alike";
}

TEST_F(AIDoctrineCatalogTest, MissingFilesLeaveTheBuiltInDoctrinesInPlace) {
  reset_ai_doctrine_catalog();
  EXPECT_FALSE(load_ai_doctrine_catalog(m_dir.filePath("nothing-here.json"),
                                        m_dir.filePath("also-missing.json")));
  EXPECT_FALSE(ai_doctrine_catalog_loaded());

  const auto profile =
      doctrine_profile_for_troop(Game::Units::TroopType::RomanLegionOrganizer);
  ASSERT_TRUE(profile.has_value());
  EXPECT_EQ(profile->strategy, AIStrategy::Defensive);
  EXPECT_EQ(profile->posture, AIPosture::Garrison);
}

TEST_F(AIDoctrineCatalogTest, MalformedJsonIsRefusedRatherThanHalfApplied) {
  reset_ai_doctrine_catalog();
  const QDir dir(m_dir.path());
  const QString bad = write_file(dir, "bad.json", QStringLiteral("{ not json at all"));
  ASSERT_FALSE(bad.isEmpty());
  EXPECT_FALSE(load_ai_doctrine_catalog(bad, {}));
  EXPECT_FALSE(ai_doctrine_catalog_loaded());
}

TEST_F(AIDoctrineCatalogTest, UnknownTownPlanFallsBackWithoutLosingTheDoctrine) {
  reset_ai_doctrine_catalog();
  const QDir dir(m_dir.path());
  const QString doctrines = write_file(dir, "doctrines.json", QStringLiteral(R"({
    "commanders": {
      "roman_veteran_consul": {
        "strategy": "aggressive",
        "town_plan": "a_plan_that_does_not_exist"
      }
    }
  })"));
  ASSERT_FALSE(doctrines.isEmpty());
  ASSERT_TRUE(load_ai_doctrine_catalog(doctrines, {}));

  const auto* doctrine = authored_doctrine("roman_veteran_consul");
  ASSERT_NE(doctrine, nullptr);
  EXPECT_EQ(doctrine->strategy, "aggressive");
  EXPECT_EQ(doctrine->town_plan, nullptr)
      << "an unknown plan name must fall back to the built-in layout, not stick";
}

TEST_F(AIDoctrineCatalogTest, UnknownWaveTargetsAreDroppedAndAnyIsAlwaysLast) {
  reset_ai_doctrine_catalog();
  const QDir dir(m_dir.path());
  const QString doctrines = write_file(dir, "doctrines.json", QStringLiteral(R"({
    "commanders": {
      "roman_veteran_consul": {
        "wave": { "target_priority": ["barracks", "carrier_pigeons"] }
      }
    }
  })"));
  ASSERT_FALSE(doctrines.isEmpty());
  ASSERT_TRUE(load_ai_doctrine_catalog(doctrines, {}));

  const auto* doctrine = authored_doctrine("roman_veteran_consul");
  ASSERT_NE(doctrine, nullptr);
  ASSERT_EQ(doctrine->wave.target_priority.size(), 2U);
  EXPECT_EQ(doctrine->wave.target_priority[0], DoctrineTarget::Barracks);
  EXPECT_EQ(doctrine->wave.target_priority[1], DoctrineTarget::Any)
      << "a wave must always have a last-resort target or it will sit still";
}

TEST_F(AIDoctrineCatalogTest, DefaultsFillTheGapsInAPartialEntry) {
  reset_ai_doctrine_catalog();
  const QDir dir(m_dir.path());
  const QString doctrines = write_file(dir, "doctrines.json", QStringLiteral(R"({
    "defaults": {
      "strategy": "defensive",
      "garrison": { "minimum_units": 7 }
    },
    "commanders": {
      "roman_veteran_consul": { "posture": "field" }
    }
  })"));
  ASSERT_FALSE(doctrines.isEmpty());
  ASSERT_TRUE(load_ai_doctrine_catalog(doctrines, {}));

  const auto* doctrine = authored_doctrine("roman_veteran_consul");
  ASSERT_NE(doctrine, nullptr);
  EXPECT_EQ(doctrine->strategy, "defensive");
  EXPECT_EQ(doctrine->posture, "field");
  EXPECT_EQ(doctrine->garrison.minimum_units, 7);
}

TEST_F(AIDoctrineCatalogTest, OutOfRangeNumbersAreClampedNotTrusted) {
  reset_ai_doctrine_catalog();
  const QDir dir(m_dir.path());
  const QString doctrines = write_file(dir, "doctrines.json", QStringLiteral(R"({
    "commanders": {
      "roman_veteran_consul": {
        "personality": { "aggression": 9.0, "defense": -4.0 },
        "wave": { "size": 100000, "spent_fraction": 5.0 },
        "garrison": { "fraction": 2.0 }
      }
    }
  })"));
  ASSERT_FALSE(doctrines.isEmpty());
  ASSERT_TRUE(load_ai_doctrine_catalog(doctrines, {}));

  const auto* doctrine = authored_doctrine("roman_veteran_consul");
  ASSERT_NE(doctrine, nullptr);
  EXPECT_LE(doctrine->aggression, 1.0F);
  EXPECT_GE(doctrine->defense, 0.0F);
  EXPECT_LE(doctrine->wave.size, 60);
  EXPECT_LE(doctrine->wave.spent_fraction, 0.95F);
  EXPECT_LE(doctrine->garrison.fraction, 0.95F);
}

TEST(AIAttackWave, AGarrisonNeverSwallowsTheWholeArmy) {
  AIContext context;

  context.strategy_config.reserve_units = 50;
  EXPECT_LE(garrison_target_for(context, 4), 3);
  EXPECT_EQ(garrison_target_for(context, 1), 0);
  EXPECT_EQ(garrison_target_for(context, 0), 0);
}

TEST(AIAttackWave, WaveSizeFallsBackToTheStrategyWithoutADoctrine) {
  AIContext context;
  context.strategy_config.doctrine = nullptr;
  context.strategy_config.proactive_attack_size = 7;
  EXPECT_EQ(wave_size_for(context), 7);
}

} // namespace
