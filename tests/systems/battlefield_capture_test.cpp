#include <algorithm>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "systems/battlefield_capture.h"

namespace Capture = Game::BattlefieldCapture;

TEST(BattlefieldCaptureTest, CatalogContainsEveryAcceptanceScene) {
  EXPECT_EQ(Capture::all_scenarios().size(), 8U);
  for (const auto id : Capture::all_scenarios()) {
    Capture::ScenarioId parsed{};
    ASSERT_TRUE(Capture::parse_scenario(Capture::scenario_name(id), parsed));
    EXPECT_EQ(parsed, id);
  }
}

TEST(BattlefieldCaptureTest, SameSeedProducesSameReplayAtEitherCaptureFps) {
  Capture::RunnerConfig config;
  config.duration_seconds = 2.0;
  config.seed = 418;
  config.capture_render_fps = 30;
  const auto at_30 = Capture::run(config);
  config.capture_render_fps = 60;
  const auto at_60 = Capture::run(config);
  EXPECT_EQ(at_30.deterministic_digest, at_60.deterministic_digest);
  EXPECT_EQ(at_30.ticks.size(), at_60.ticks.size());
}

TEST(BattlefieldCaptureTest, TelemetryIncludesRequiredTickAndOverlayChannels) {
  Capture::RunnerConfig config;
  config.duration_seconds = 0.5;
  const auto capture = Capture::run(config);
  ASSERT_FALSE(capture.ticks.empty());
  EXPECT_GT(capture.performance.entity_updates, 0U);
  for (const std::string kind : {"group_anchor", "formation_slot"}) {
    EXPECT_NE(std::find_if(capture.overlays.begin(),
                           capture.overlays.end(),
                           [&](const auto& item) { return item.kind == kind; }),
              capture.overlays.end())
        << kind;
  }
  std::ostringstream stream;
  Capture::write_json_lines(capture, stream);
  const auto log = stream.str();
  EXPECT_NE(log.find("\"transform\""), std::string::npos);
  EXPECT_NE(log.find("\"animation\""), std::string::npos);
  EXPECT_NE(log.find("\"digest\""), std::string::npos);
}

TEST(BattlefieldCaptureTest, ProductionWorldScenarioPassesGameplayQualityGate) {
  Capture::RunnerConfig config;
  config.duration_seconds = 3.0;
  auto const capture = Capture::run(config);
  auto const report = Capture::verify(capture);
  EXPECT_GT(capture.performance.entity_updates, 0U);
  EXPECT_GE(capture.quality.first_response_seconds, 0.0);
  EXPECT_TRUE(report.passed) << (report.issues.empty() ? ""
                                                       : report.issues.front().message);
}

TEST(BattlefieldCaptureTest, DefaultRunIsSixtySecondSoak) {
  const Capture::RunnerConfig config;
  EXPECT_DOUBLE_EQ(config.duration_seconds, 60.0);
  EXPECT_DOUBLE_EQ(config.fixed_step_seconds, 1.0 / 30.0);
}
