

#include <QDir>
#include <QFileInfo>

#include <gtest/gtest.h>

#include "tools/balance_sim/balance_fixture.h"
#include "tools/balance_sim/balance_report.h"
#include "tools/balance_sim/battle_simulation.h"

namespace {

auto fixture_directory() -> QString {
  const QStringList candidates{
      QDir::current().filePath(QStringLiteral("assets/balance")),
      QDir::current().filePath(QStringLiteral("../assets/balance")),
      QDir::current().filePath(QStringLiteral("../../assets/balance")),
  };
  for (const QString& candidate : candidates) {
    if (QDir(candidate).exists()) {
      return QDir(candidate).absolutePath();
    }
  }
  return {};
}

auto load(const QString& id) -> Balance::Fixture {
  const QString directory = fixture_directory();
  EXPECT_FALSE(directory.isEmpty()) << "assets/balance not found from the test cwd";
  std::vector<Balance::FixtureLoadError> errors;
  auto fixtures = Balance::load_fixture_directory(directory, errors);
  for (const auto& error : errors) {
    ADD_FAILURE() << error.field.toStdString() << ": " << error.message.toStdString();
  }
  for (auto& fixture : fixtures) {
    if (fixture.id == id) {
      return fixture;
    }
  }
  ADD_FAILURE() << "no balance fixture with id " << id.toStdString();
  return {};
}

auto run(Balance::Fixture fixture, int seeds) -> Balance::FixtureSummary {
  fixture.seeds = seeds;
  std::vector<Balance::BattleResult> results;
  for (int seed = 0; seed < fixture.seeds; ++seed) {
    const auto value = static_cast<std::uint32_t>(seed) * 0x9E3779B9U + 1U;
    results.push_back(Balance::run_battle(fixture, value, false));
    if (fixture.mirror_sides) {
      results.push_back(Balance::run_battle(fixture, value, true));
    }
  }
  return Balance::summarize(fixture, std::move(results));
}

class BalanceSimTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { Balance::initialize_simulation_environment(); }
};

TEST_F(BalanceSimTest, EveryShippedFixtureParses) {
  const QString directory = fixture_directory();
  ASSERT_FALSE(directory.isEmpty());
  std::vector<Balance::FixtureLoadError> errors;
  const auto fixtures = Balance::load_fixture_directory(directory, errors);
  EXPECT_TRUE(errors.empty());
  EXPECT_GE(fixtures.size(), 8U);
}

TEST_F(BalanceSimTest, SameSeedProducesTheSameBattle) {
  const auto fixture = load(QStringLiteral("mirror_swordsman"));
  const auto first = Balance::run_battle(fixture, 12345U, false);
  const auto second = Balance::run_battle(fixture, 12345U, false);

  EXPECT_EQ(first.outcome, second.outcome);
  EXPECT_FLOAT_EQ(first.elapsed_seconds, second.elapsed_seconds);
  EXPECT_EQ(first.side_a.surviving_units, second.side_a.surviving_units);
  EXPECT_EQ(first.side_b.surviving_units, second.side_b.surviving_units);
  EXPECT_DOUBLE_EQ(first.side_a.damage_dealt.total(),
                   second.side_a.damage_dealt.total());
}

TEST_F(BalanceSimTest, DifferentSeedsProduceDifferentBattles) {
  const auto fixture = load(QStringLiteral("faction_line_rome_vs_carthage"));
  const auto first = Balance::run_battle(fixture, 1U, false);
  const auto second = Balance::run_battle(fixture, 999U, false);
  EXPECT_NE(first.elapsed_seconds, second.elapsed_seconds);
}

TEST_F(BalanceSimTest, EvenInfantryFightResolvesInsteadOfStalling) {
  const auto summary = run(load(QStringLiteral("mirror_swordsman")), 3);
  EXPECT_DOUBLE_EQ(summary.timeout_rate, 0.0);
  EXPECT_GT(summary.median_victory_seconds, 10.0);
  EXPECT_LT(summary.median_victory_seconds, 90.0);

  EXPECT_NEAR(summary.a_win_rate, 0.5, 0.2);
}

TEST_F(BalanceSimTest, InfantryOverrunsUnescortedSiege) {
  const auto summary = run(load(QStringLiteral("infantry_vs_siege")), 2);
  EXPECT_GE(summary.a_win_rate, 0.85);
  EXPECT_LT(summary.median_victory_seconds, 45.0);
}

TEST_F(BalanceSimTest, BracedSpearsBeatAFrontalCavalryCharge) {
  const auto summary = run(load(QStringLiteral("spear_vs_cavalry_frontal")), 2);
  EXPECT_GE(summary.a_win_rate, 0.6);
}

TEST_F(BalanceSimTest, CavalryOverrunsExposedArchers) {
  const auto summary = run(load(QStringLiteral("cavalry_vs_exposed_archers")), 2);
  EXPECT_GE(summary.a_win_rate, 0.6);
}

TEST_F(BalanceSimTest, EliteCommanderLosesToEqualCostLineInfantry) {
  const auto summary = run(load(QStringLiteral("commander_vs_line")), 12);
  EXPECT_LE(summary.a_win_rate, 0.5);

  EXPECT_GT(summary.side_a.mean_damage_melee + summary.side_a.mean_damage_ranged,
            500.0);
}

TEST_F(BalanceSimTest, FactionsAreEvenAtEqualCost) {
  const auto summary = run(load(QStringLiteral("faction_line_rome_vs_carthage")), 4);
  EXPECT_GE(summary.a_win_rate, 0.3);
  EXPECT_LE(summary.a_win_rate, 0.7);
  EXPECT_LE(summary.timeout_rate, 0.25);
}

TEST_F(BalanceSimTest, NoFriendlyFireAcrossTheWholeMatrix) {
  for (const QString& id : {QStringLiteral("faction_line_rome_vs_carthage"),
                            QStringLiteral("undead_vs_rome"),
                            QStringLiteral("archers_vs_elephant")}) {
    const auto summary = run(load(id), 1);
    EXPECT_EQ(summary.invalid.friendly_fire_hits, 0U) << id.toStdString();
  }
}

} // namespace
