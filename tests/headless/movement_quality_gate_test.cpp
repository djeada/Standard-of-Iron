#include <QString>

#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <vector>

#include "core/movement_trace.h"
#include "core/movement_trace_analysis.h"
#include "game/session/session_context.h"
#include "game/systems/battlefield_capture.h"

namespace {

using Engine::Core::MovementFindingKind;

struct ScenarioBudget {
  Game::BattlefieldCapture::ScenarioId scenario;
  int max_findings;
};

auto manifest_for(const char* scenario) -> Engine::Core::MovementTraceManifest {
  Engine::Core::MovementTraceManifest manifest;
  manifest.scenario = scenario;
  manifest.fixed_step_seconds = 1.0F / 60.0F;
  manifest.map_id = "battlefield_capture";
  return manifest;
}

auto analyse(Game::BattlefieldCapture::ScenarioId scenario,
             double seconds) -> Engine::Core::MovementAnalysis {
  Engine::Core::ScopedMovementTrace const session(
      manifest_for(Game::BattlefieldCapture::scenario_name(scenario)));

  Game::BattlefieldCapture::RunnerConfig config;
  config.scenario = scenario;
  config.duration_seconds = seconds;
  (void)Game::BattlefieldCapture::run(config);

  auto& trace = Engine::Core::MovementTrace::instance();
  Engine::Core::MovementGateThresholds thresholds;
  return Engine::Core::analyze_movement_trace(
      trace.troop_samples(), trace.soldier_samples(), thresholds);
}

auto count_of(const Engine::Core::MovementAnalysis& analysis,
              MovementFindingKind kind) -> int {
  return static_cast<int>(analysis.count(kind));
}

} // namespace

class MovementQualityGateTest : public ::testing::TestWithParam<ScenarioBudget> {};

TEST_P(MovementQualityGateTest, AScenarioWalksWithoutFightingItself) {
  const auto budget = GetParam();
  const auto analysis = analyse(budget.scenario, 30.0);

  const std::string name = Game::BattlefieldCapture::scenario_name(budget.scenario);
  ASSERT_GT(analysis.entities.size(), 0U) << name << " recorded no movement at all";

  EXPECT_LE(static_cast<int>(analysis.findings.size()), budget.max_findings)
      << name << " produced " << analysis.findings.size()
      << " movement findings, budget " << budget.max_findings << ":\n"
      << Engine::Core::format_movement_findings(analysis)
      << Engine::Core::format_movement_summary(analysis);
}

TEST_P(MovementQualityGateTest, NoBodyWalksOnTheSpotForLong) {
  const auto analysis = analyse(GetParam().scenario, 30.0);

  float worst = 0.0F;
  for (const auto& finding : analysis.findings) {
    if (finding.kind == MovementFindingKind::GaitWithoutMotion) {
      worst = std::max(worst, finding.magnitude);
    }
  }

  EXPECT_LT(worst, 4.0F) << Game::BattlefieldCapture::scenario_name(GetParam().scenario)
                         << ": a body played its walk cycle for " << worst
                         << "s without moving; that is the treadmill a player sees";
}

TEST_P(MovementQualityGateTest, NoOrderIsAbandonedUnresolved) {
  const auto analysis = analyse(GetParam().scenario, 30.0);

  EXPECT_EQ(count_of(analysis, MovementFindingKind::Starvation), 0)
      << Game::BattlefieldCapture::scenario_name(GetParam().scenario)
      << ": an order stayed active without ever resolving";
  EXPECT_EQ(count_of(analysis, MovementFindingKind::MissingTerminalOutcome), 0)
      << Game::BattlefieldCapture::scenario_name(GetParam().scenario)
      << ": an order stalled and was never resolved";
}

TEST_P(MovementQualityGateTest, NobodyIsLeftPressingIntoGeometry) {
  const auto analysis = analyse(GetParam().scenario, 30.0);

  const std::string name = Game::BattlefieldCapture::scenario_name(GetParam().scenario);
  EXPECT_EQ(count_of(analysis, MovementFindingKind::CollisionPenetration), 0)
      << name << ": a body ended up inside geometry";
  EXPECT_EQ(count_of(analysis, MovementFindingKind::WaypointRegression), 0)
      << name << ": a route walked backwards through its own waypoints";
  EXPECT_EQ(count_of(analysis, MovementFindingKind::BodyOverlap), 0)
      << Game::BattlefieldCapture::scenario_name(GetParam().scenario)
      << ": bodies were left standing inside one another\n"
      << Engine::Core::format_movement_findings(analysis);

  EXPECT_EQ(count_of(analysis, MovementFindingKind::BlockedStepStreak), 0)
      << name << ": a body pressed into something it could not pass";
}

INSTANTIATE_TEST_SUITE_P(
    CaptureScenarios,
    MovementQualityGateTest,
    ::testing::Values(
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::InfantryApproach20v20, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::ArchersVsInfantry, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::MixedFormation, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::CasualtyReflow, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::CavalryCharge, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::NarrowPassage, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::CommanderInLine, 20},
        ScenarioBudget{Game::BattlefieldCapture::ScenarioId::BotSkirmish, 20}));
