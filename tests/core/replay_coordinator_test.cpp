#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "app/session/replay_coordinator.h"
#include "game/session/session_context.h"

namespace {

using App::Session::ReplayCoordinator;
using App::Session::ReplayLaunch;
using App::Session::ReplayPlaybackResult;
using App::Session::ReplayStarters;

struct Calls {
  QString campaign_reference;
  QString mission_reference;
  QString skirmish_reference;
  QVariantList skirmish_configs;
  QString difficulty;
  int total = 0;
};

auto recording_starters(Calls& calls) -> ReplayStarters {
  return {.campaign_mission =
              [&calls](const QString& reference, const QString& difficulty) {
                calls.campaign_reference = reference;
                calls.difficulty = difficulty;
                ++calls.total;
              },
          .mission_file =
              [&calls](const QString& reference, const QString& difficulty) {
                calls.mission_reference = reference;
                calls.difficulty = difficulty;
                ++calls.total;
              },
          .skirmish =
              [&calls](const QString& reference, const QVariantList& configs) {
                calls.skirmish_reference = reference;
                calls.skirmish_configs = configs;
                ++calls.total;
              }};
}

auto record_launch(const QString& path, const ReplayLaunch& launch) -> bool {
  Game::Session::SessionContext session;
  ReplayCoordinator coordinator;
  coordinator.set_record_path(path);
  coordinator.note_launch(launch);
  coordinator.arm_for_started_match(&session);
  auto* recorder = session.replay_recorder();
  if (recorder == nullptr) {
    return false;
  }
  recorder->finish();
  return true;
}

TEST(ReplayCoordinatorTest, ArmingWithNothingRequestedLeavesTheSessionAlone) {
  Game::Session::SessionContext session;
  ReplayCoordinator coordinator;
  coordinator.arm_for_started_match(&session);
  EXPECT_EQ(session.replay_recorder(), nullptr);
  EXPECT_EQ(session.replay_player(), nullptr);
  coordinator.arm_for_started_match(nullptr);
}

TEST(ReplayCoordinatorTest, AnUnreadableReplayReportsALoadFailureAndStartsNothing) {
  ReplayCoordinator coordinator;
  Calls calls;
  const auto result = coordinator.begin_playback(
      QStringLiteral("/nonexistent/replay.soi"), recording_starters(calls));
  EXPECT_EQ(result.failure, ReplayPlaybackResult::Failure::LoadFailed);
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.detail.isEmpty());
  EXPECT_EQ(calls.total, 0);
}

TEST(ReplayCoordinatorTest, ARecordedSkirmishIsReplayedThroughItsOwnStarter) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("skirmish.replay"));
  QVariantMap config;
  config["player_id"] = 2;
  ASSERT_TRUE(record_launch(path,
                            {.kind = QStringLiteral("skirmish"),
                             .reference = QStringLiteral("maps/arena.json"),
                             .player_configs = {config},
                             .difficulty = QStringLiteral("hard")}));

  ReplayCoordinator coordinator;
  Calls calls;
  const auto result = coordinator.begin_playback(path, recording_starters(calls));
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(calls.total, 1);
  EXPECT_EQ(calls.skirmish_reference, QStringLiteral("maps/arena.json"));
  ASSERT_EQ(calls.skirmish_configs.size(), 1);
  EXPECT_EQ(calls.skirmish_configs.front().toMap().value("player_id").toInt(), 2);

  Game::Session::SessionContext session;
  coordinator.arm_for_started_match(&session);
  EXPECT_NE(session.replay_player(), nullptr) << "the pending replay drives the match";
  EXPECT_EQ(session.replay_recorder(), nullptr);

  Game::Session::SessionContext next_match;
  coordinator.arm_for_started_match(&next_match);
  EXPECT_EQ(next_match.replay_player(), nullptr) << "a replay drives one match only";
}

TEST(ReplayCoordinatorTest, CampaignReplaysCarryTheirDifficultyToTheStarter) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("campaign.replay"));
  ASSERT_TRUE(record_launch(path,
                            {.kind = QStringLiteral("campaign-mission"),
                             .reference = QStringLiteral("campaign/first.json"),
                             .player_configs = {},
                             .difficulty = QStringLiteral("hard")}));

  ReplayCoordinator coordinator;
  Calls calls;
  ASSERT_TRUE(coordinator.begin_playback(path, recording_starters(calls)).ok());
  EXPECT_EQ(calls.campaign_reference, QStringLiteral("campaign/first.json"));
  EXPECT_EQ(calls.total, 1);
  EXPECT_FALSE(calls.difficulty.isEmpty());
}

TEST(ReplayCoordinatorTest, AnUnknownLaunchKindIsRefusedAndForgotten) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("odd.replay"));
  ASSERT_TRUE(record_launch(path,
                            {.kind = QStringLiteral("hologram"),
                             .reference = QStringLiteral("x"),
                             .player_configs = {},
                             .difficulty = QString()}));

  ReplayCoordinator coordinator;
  Calls calls;
  const auto result = coordinator.begin_playback(path, recording_starters(calls));
  EXPECT_EQ(result.failure, ReplayPlaybackResult::Failure::UnknownKind);
  EXPECT_EQ(result.detail, QStringLiteral("hologram"));
  EXPECT_EQ(calls.total, 0);

  Game::Session::SessionContext session;
  coordinator.arm_for_started_match(&session);
  EXPECT_EQ(session.replay_player(), nullptr);
}

TEST(ReplayCoordinatorTest, RecordingIsArmedFromTheNoteLaunchAndPath) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  Game::Session::SessionContext session;
  ReplayCoordinator coordinator;
  coordinator.set_record_path(dir.filePath(QStringLiteral("rec.replay")));
  coordinator.note_launch({.kind = QStringLiteral("skirmish"),
                           .reference = QStringLiteral("m.json"),
                           .player_configs = {},
                           .difficulty = QString()});
  coordinator.arm_for_started_match(&session);
  ASSERT_NE(session.replay_recorder(), nullptr);
  EXPECT_TRUE(session.replay_recorder()->recording());
  EXPECT_EQ(session.replay_player(), nullptr);
}

} // namespace
