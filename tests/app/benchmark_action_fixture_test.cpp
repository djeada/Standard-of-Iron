#include <QJsonDocument>
#include <QString>

#include <gtest/gtest.h>

#include "app/core/benchmark_action_fixture.h"

namespace {

using App::Core::actions_between;
using App::Core::BenchmarkActionFixture;
using App::Core::parse_benchmark_action_fixture;

auto parse(const char* json,
           QString* error = nullptr) -> std::optional<BenchmarkActionFixture> {
  return parse_benchmark_action_fixture(QJsonDocument::fromJson(QByteArray(json)),
                                        error);
}

constexpr const char* k_minimal = R"({
  "version": 1,
  "name": "minimal",
  "loop_seconds": 10.0,
  "required_coverage": ["formation_move", "selection_change"],
  "actions": [
    {"at": 5.0, "action": "move_to", "x": 0.6, "y": 0.4},
    {"at": 1.0, "action": "select_all"}
  ]
})";

TEST(BenchmarkActionFixtureTest, AValidFixtureParsesAndSortsItsActions) {
  QString error;
  const auto fixture = parse(k_minimal, &error);
  ASSERT_TRUE(fixture.has_value()) << error.toStdString();
  EXPECT_EQ(fixture->name, QStringLiteral("minimal"));
  EXPECT_EQ(fixture->version, 1);
  EXPECT_DOUBLE_EQ(fixture->loop_seconds, 10.0);
  ASSERT_EQ(fixture->actions.size(), 2U);
  EXPECT_EQ(fixture->actions[0].action, QStringLiteral("select_all"));
  EXPECT_EQ(fixture->actions[1].action, QStringLiteral("move_to"));
  EXPECT_DOUBLE_EQ(fixture->actions[1].x, 0.6);
}

TEST(BenchmarkActionFixtureTest, AnUnsupportedVersionIsRejected) {
  QString error;
  EXPECT_FALSE(parse(R"({"version": 2, "name": "x",
      "required_coverage": ["formation_move"],
      "actions": [{"at": 0.5, "action": "select_all"}]})",
                     &error)
                   .has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("version")));
}

TEST(BenchmarkActionFixtureTest, AnUnknownActionIsRejected) {
  QString error;
  EXPECT_FALSE(parse(R"({"version": 1, "name": "x", "required_coverage": ["idle"],
      "actions": [{"at": 0.5, "action": "teleport"}]})",
                     &error)
                   .has_value());
}

TEST(BenchmarkActionFixtureTest, AnUnknownRequiredCoverageEventIsRejected) {
  QString error;
  EXPECT_FALSE(parse(R"({"version": 1, "name": "x",
      "required_coverage": ["invent_a_behaviour"],
      "actions": [{"at": 0.5, "action": "select_all"}]})",
                     &error)
                   .has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("coverage")));
}

TEST(BenchmarkActionFixtureTest, AFixtureWithoutRequiredCoverageIsRejected) {
  EXPECT_FALSE(parse(R"({"version": 1, "name": "x", "required_coverage": [],
      "actions": [{"at": 0.5, "action": "select_all"}]})")
                   .has_value());
}

TEST(BenchmarkActionFixtureTest, AnActionOutsideTheLoopIsRejected) {
  QString error;
  EXPECT_FALSE(parse(R"({"version": 1, "name": "x", "loop_seconds": 4.0,
      "required_coverage": ["formation_move"],
      "actions": [{"at": 9.0, "action": "select_all"}]})",
                     &error)
                   .has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("loop")));
}

TEST(BenchmarkActionFixtureTest, EachActionFiresExactlyOncePerLoop) {
  const auto fixture = parse(k_minimal);
  ASSERT_TRUE(fixture.has_value());

  int selects = 0;
  int moves = 0;
  double previous = 0.0;
  for (int step = 1; step <= 1250; ++step) {
    const double now = static_cast<double>(step) * 0.016;
    for (const auto& action : actions_between(*fixture, previous, now)) {
      selects += action.action == QStringLiteral("select_all") ? 1 : 0;
      moves += action.action == QStringLiteral("move_to") ? 1 : 0;
    }
    previous = now;
  }
  EXPECT_EQ(selects, 2) << "two whole 10 s loops fit in 20 s";
  EXPECT_EQ(moves, 2);
}

TEST(BenchmarkActionFixtureTest, ANonLoopingFixtureFiresEachActionOnce) {
  const auto fixture = parse(R"({"version": 1, "name": "once",
      "required_coverage": ["selection_change"],
      "actions": [{"at": 1.0, "action": "select_all"},
                  {"at": 2.0, "action": "stop"}]})");
  ASSERT_TRUE(fixture.has_value());

  int fired = 0;
  double previous = 0.0;
  for (int step = 1; step <= 600; ++step) {
    const double now = static_cast<double>(step) * 0.016;
    fired += static_cast<int>(actions_between(*fixture, previous, now).size());
    previous = now;
  }
  EXPECT_EQ(fired, 2);
}

TEST(BenchmarkActionFixtureTest, ATimeStepThatSkipsBackwardsFiresNothing) {
  const auto fixture = parse(k_minimal);
  ASSERT_TRUE(fixture.has_value());
  EXPECT_TRUE(actions_between(*fixture, 5.0, 5.0).empty());
  EXPECT_TRUE(actions_between(*fixture, 5.0, 1.0).empty());
}

} // namespace
