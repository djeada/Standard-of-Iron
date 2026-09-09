

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

TEST_F(AIDoctrineCatalogTest, TownPlanStepsCarryAnOptionalRotation) {
  reset_ai_doctrine_catalog();
  const QDir dir(m_dir.path());
  const QString plans = write_file(dir, "town_plans.json", QStringLiteral(R"({
    "plans": {
      "ring": {
        "steps": [
          { "building": "wall_segment", "x": 0.0, "z": -12.0, "rotation": 30.0 },
          { "building": "wall_gate", "x": 4.0, "z": -12.0 },
          { "building": "home", "x": 0.0, "z": 12.0 }
        ]
      }
    }
  })"));
  ASSERT_FALSE(plans.isEmpty());
  const QString doctrines = write_file(dir, "doctrines.json", QStringLiteral(R"({
    "commanders": { "roman_veteran_consul": { "town_plan": "ring" } }
  })"));
  ASSERT_TRUE(load_ai_doctrine_catalog(doctrines, plans));

  const auto* plan = authored_town_plan("ring");
  ASSERT_NE(plan, nullptr);
  ASSERT_EQ(plan->steps.size(), 3U);
  EXPECT_FLOAT_EQ(plan->steps[0].rotation, 30.0F);
  EXPECT_FLOAT_EQ(plan->steps[1].rotation, 0.0F) << "rotation defaults to 0";
  EXPECT_EQ(plan->wall_step_count(), 2);
}

TEST_F(AIDoctrineCatalogTest, ShippedTownPlansKeepClearOfTheBaseAnchor) {
  reset_ai_doctrine_catalog();
  ASSERT_TRUE(load_default_ai_doctrine_catalog());

  int walled_plans = 0;
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto* doctrine = authored_doctrine(definition.id);
    ASSERT_NE(doctrine, nullptr);
    ASSERT_NE(doctrine->town_plan, nullptr);
    if (doctrine->town_plan->wall_step_count() > 0) {
      ++walled_plans;
    }
    for (const auto& step : doctrine->town_plan->steps) {
      EXPECT_GE(std::hypot(step.x, step.z), TownPlan::k_anchor_clearance)
          << definition.id << " places " << step.building << " inside the base anchor";
    }
  }
  EXPECT_GE(walled_plans, 3) << "square, star and ring towns all need walls";
}

namespace {

auto fortification_offsets(const TownPlan& plan,
                           int first_steps) -> std::vector<TownPlanOffset> {
  std::vector<TownPlanOffset> offsets;
  const int limit = std::min(first_steps, static_cast<int>(plan.steps.size()));
  for (int slot = 0; slot < limit; ++slot) {
    const auto& step = plan.steps[static_cast<std::size_t>(slot)];
    if (step.building == "wall_segment" || step.building == "wall_gate" ||
        step.building == "defense_tower") {
      offsets.push_back({step.x, step.z});
    }
  }
  return offsets;
}

} // namespace

TEST_F(AIDoctrineCatalogTest, EachShippedPlanDerivesItsAuthoredForm) {
  reset_ai_doctrine_catalog();
  ASSERT_TRUE(load_default_ai_doctrine_catalog());

  const std::vector<std::pair<const char*, SettlementForm>> expected{
      {"roman_bulwark", SettlementForm::ClosedFort},
      {"roman_assault_camp", SettlementForm::ClosedFort},
      {"roman_vanguard_camp", SettlementForm::OpenCamp},
      {"punic_trade_town", SettlementForm::RingTown},
      {"punic_raider_camp", SettlementForm::OpenCamp},
      {"punic_grand_camp", SettlementForm::ClosedFort},
  };
  for (const auto& [id, form] : expected) {
    const auto* plan = authored_town_plan(id);
    ASSERT_NE(plan, nullptr) << id;
    EXPECT_EQ(plan->form(), form)
        << id << " reads as " << settlement_form_name(plan->form()) << ", not "
        << settlement_form_name(form);
  }
}

TEST_F(AIDoctrineCatalogTest, ASilhouetteAlreadyDrawsTheOutline) {
  reset_ai_doctrine_catalog();
  ASSERT_TRUE(load_default_ai_doctrine_catalog());

  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto* plan = authored_doctrine(definition.id)->town_plan;
    ASSERT_NE(plan, nullptr);
    EXPECT_GT(plan->silhouette_steps, 0) << plan->id << " declares no silhouette";
    EXPECT_LE(plan->silhouette_steps,
              std::max(10, static_cast<int>(plan->steps.size()) * 45 / 100))
        << plan->id << " calls most of itself its silhouette";
    if (plan->form() == SettlementForm::OpenCamp) {
      continue;
    }
    const auto silhouette = fortification_offsets(*plan, plan->silhouette_steps);
    EXPECT_GE(TownPlan::compass_coverage(silhouette), 0.70F)
        << plan->id << "'s silhouette leaves most of the compass open";
    const auto* gate = plan->front_gate();
    ASSERT_NE(gate, nullptr) << plan->id;
    const auto gate_slot = static_cast<int>(gate - plan->steps.data());
    EXPECT_TRUE(plan->is_silhouette_step(gate_slot))
        << plan->id << " leaves its front gate for later";
  }
}

TEST_F(AIDoctrineCatalogTest, MustersKeepClearOfThePlanAndSitOnTheFrontAxis) {
  reset_ai_doctrine_catalog();
  ASSERT_TRUE(load_default_ai_doctrine_catalog());

  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const auto* plan = authored_doctrine(definition.id)->town_plan;
    ASSERT_NE(plan, nullptr);
    const float front = plan->front_line_z();
    EXPECT_LT(front, -TownPlan::k_anchor_clearance) << plan->id;

    const auto inside = plan->muster_offset(MusterSide::Inside);
    const auto outside = plan->muster_offset(MusterSide::Outside);
    EXPECT_GT(inside.z, front) << plan->id << " musters its ward outside its wall";
    EXPECT_LT(outside.z, front) << plan->id << " musters its field inside its wall";
    EXPECT_GE(std::hypot(inside.x, inside.z), TownPlan::k_anchor_clearance) << plan->id;

    float nearest_slot = 1000.0F;
    for (const auto& step : plan->steps) {
      nearest_slot =
          std::min(nearest_slot, std::hypot(step.x - inside.x, step.z - inside.z));
    }
    EXPECT_GE(nearest_slot, 2.5F)
        << plan->id << " forms its army on top of a planned building";
  }
}
