#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>

#include "map/campaign_definition.h"
#include "map/campaign_loader.h"
#include "systems/save_storage.h"

// End-to-end coverage for issue #1079: walking a campaign from its first
// mission to its last and back, and checking that progression survives replays,
// reopened databases and stale requests.
//
// These are deliberately written against the storage layer rather than the
// engine: it is the only place that decides what "unlocked", "completed" and
// "campaign finished" mean, and it can be driven headlessly one mission at a
// time in the order a player would actually clear them.
namespace {

using namespace Game::Systems;

auto find_repo_root() -> QDir {
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("assets/campaigns/second_punic_war.json"))) {
      return dir;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::current();
}

auto shipped_campaign() -> Game::Campaign::CampaignDefinition {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  const QString path = find_repo_root().filePath(
      QStringLiteral("assets/campaigns/second_punic_war.json"));
  EXPECT_TRUE(
      Game::Campaign::CampaignLoader::load_from_json_file(path, campaign, &error))
      << error.toStdString();
  return campaign;
}

auto mission_row(const QVariantList& progress,
                 const QString& mission_id) -> QVariantMap {
  for (const QVariant& entry : progress) {
    const QVariantMap row = entry.toMap();
    if (row.value(QStringLiteral("mission_id")).toString() == mission_id) {
      return row;
    }
  }
  return {};
}

auto is_unlocked(const QVariantList& progress, const QString& mission_id) -> bool {
  return mission_row(progress, mission_id).value(QStringLiteral("unlocked")).toBool();
}

auto is_completed(const QVariantList& progress, const QString& mission_id) -> bool {
  return mission_row(progress, mission_id).value(QStringLiteral("completed")).toBool();
}

class CampaignProgressionTest : public ::testing::Test {
protected:
  void SetUp() override {
    campaign = shipped_campaign();
    ASSERT_FALSE(campaign.missions.empty()) << "the shipped campaign has no missions";

    storage = std::make_unique<SaveStorage>(QStringLiteral(":memory:"));
    QString error;
    ASSERT_TRUE(storage->initialize(&error)) << error.toStdString();
    ASSERT_TRUE(storage->ensure_campaign_missions_in_db(campaign, &error))
        << error.toStdString();
  }

  void TearDown() override { storage.reset(); }

  [[nodiscard]] auto progress() const -> QVariantList {
    return storage->get_campaign_mission_progress(campaign.id);
  }

  [[nodiscard]] auto mission_at(std::size_t index) const -> QString {
    return campaign.missions[index].mission_id;
  }

  [[nodiscard]] auto last_mission() const -> QString {
    return campaign.missions.back().mission_id;
  }

  auto complete(const QString& mission_id,
                QString* error = nullptr) -> std::optional<CampaignAdvance> {
    QString local_error;
    auto advance = storage->complete_campaign_mission(
        campaign.id, mission_id, error != nullptr ? error : &local_error);
    return advance;
  }

  Game::Campaign::CampaignDefinition campaign;
  std::unique_ptr<SaveStorage> storage;
};

TEST_F(CampaignProgressionTest, OnlyTheOpeningMissionStartsUnlocked) {
  const auto rows = progress();
  ASSERT_EQ(static_cast<std::size_t>(rows.size()), campaign.missions.size());

  EXPECT_TRUE(is_unlocked(rows, mission_at(0)));
  for (std::size_t i = 1; i < campaign.missions.size(); ++i) {
    EXPECT_FALSE(is_unlocked(rows, mission_at(i)))
        << mission_at(i).toStdString() << " should still be locked at a fresh start";
  }
  for (const auto& mission : campaign.missions) {
    EXPECT_FALSE(is_completed(rows, mission.mission_id));
  }
}

TEST_F(CampaignProgressionTest, EachWinUnlocksExactlyTheNextMission) {
  for (std::size_t i = 0; i + 1 < campaign.missions.size(); ++i) {
    const auto advance = complete(mission_at(i));
    ASSERT_TRUE(advance.has_value()) << "completing " << mission_at(i).toStdString();
    EXPECT_EQ(advance->unlocked_mission_id, mission_at(i + 1));
    EXPECT_FALSE(advance->campaign_completed)
        << "the campaign ended at mission " << i << " of " << campaign.missions.size();

    const auto rows = progress();
    for (std::size_t j = 0; j < campaign.missions.size(); ++j) {
      const bool should_be_unlocked = j <= i + 1;
      EXPECT_EQ(is_unlocked(rows, mission_at(j)), should_be_unlocked)
          << mission_at(j).toStdString() << " after clearing " << i;
      EXPECT_EQ(is_completed(rows, mission_at(j)), j <= i)
          << mission_at(j).toStdString() << " after clearing " << i;
    }
  }
}

TEST_F(CampaignProgressionTest, ClearingTheFinalMissionCompletesTheCampaign) {
  for (std::size_t i = 0; i + 1 < campaign.missions.size(); ++i) {
    ASSERT_TRUE(complete(mission_at(i)).has_value());
  }

  QString error;
  const auto advance = complete(last_mission(), &error);

  // The old path rolled the whole transaction back when there was no next
  // mission to unlock, so the finale never recorded as completed and the
  // campaign could not be finished at all.
  ASSERT_TRUE(advance.has_value()) << error.toStdString();
  EXPECT_TRUE(advance->unlocked_mission_id.isEmpty())
      << "there is nothing after the last mission to unlock";
  EXPECT_TRUE(advance->campaign_completed);
  EXPECT_TRUE(is_completed(progress(), last_mission()));

  const QVariantMap campaign_progress = storage->get_campaign_progress(campaign.id);
  EXPECT_TRUE(campaign_progress.value(QStringLiteral("completed")).toBool());
}

TEST_F(CampaignProgressionTest, TheCampaignIsNotCompleteUntilEveryMissionIs) {
  // Clearing the finale out of order (however the player got there) must not
  // paper over the missions they skipped.
  ASSERT_TRUE(complete(mission_at(0)).has_value());
  const auto advance = complete(last_mission());

  ASSERT_TRUE(advance.has_value());
  EXPECT_FALSE(advance->campaign_completed);
  EXPECT_FALSE(storage->get_campaign_progress(campaign.id)
                   .value(QStringLiteral("completed"))
                   .toBool());
}

TEST_F(CampaignProgressionTest, AMidCampaignWinNeverCompletesTheCampaign) {
  // The engine used to mark the campaign finished on every victory, so the
  // first mission ended the war.
  const auto advance = complete(mission_at(0));

  ASSERT_TRUE(advance.has_value());
  EXPECT_FALSE(advance->campaign_completed);
  EXPECT_TRUE(storage->get_campaign_progress(campaign.id).isEmpty())
      << "nothing should have been written to campaign progress yet";
}

TEST_F(CampaignProgressionTest, ReplayingAnEarlierMissionDoesNotRollBackProgress) {
  for (const auto& mission : campaign.missions) {
    ASSERT_TRUE(complete(mission.mission_id).has_value());
  }
  ASSERT_TRUE(storage->get_campaign_progress(campaign.id)
                  .value(QStringLiteral("completed"))
                  .toBool());

  const auto replay = complete(mission_at(0));
  ASSERT_TRUE(replay.has_value());
  EXPECT_FALSE(replay->newly_completed) << "a replay is not a first completion";
  EXPECT_TRUE(replay->campaign_completed);

  const auto rows = progress();
  for (const auto& mission : campaign.missions) {
    EXPECT_TRUE(is_completed(rows, mission.mission_id))
        << mission.mission_id.toStdString() << " lost its completion on a replay";
    EXPECT_TRUE(is_unlocked(rows, mission.mission_id));
  }
  EXPECT_TRUE(storage->get_campaign_progress(campaign.id)
                  .value(QStringLiteral("completed"))
                  .toBool());
}

TEST_F(CampaignProgressionTest, WinningTheSameMissionTwiceIsIdempotent) {
  const auto first = complete(mission_at(0));
  ASSERT_TRUE(first.has_value());
  EXPECT_TRUE(first->newly_completed);

  const auto second = complete(mission_at(0));
  ASSERT_TRUE(second.has_value()) << "a duplicate victory signal must not fail";
  EXPECT_FALSE(second->newly_completed);
  EXPECT_EQ(second->unlocked_mission_id, first->unlocked_mission_id);

  const auto rows = progress();
  int unlocked_count = 0;
  for (const auto& mission : campaign.missions) {
    unlocked_count += is_unlocked(rows, mission.mission_id) ? 1 : 0;
  }
  EXPECT_EQ(unlocked_count, 2) << "a repeated win advanced the campaign twice";
}

TEST_F(CampaignProgressionTest, AMissionOutsideTheCampaignIsRefused) {
  QString error;
  const auto advance = complete(QStringLiteral("mission_that_was_deleted"), &error);

  EXPECT_FALSE(advance.has_value());
  EXPECT_FALSE(error.isEmpty());

  const auto rows = progress();
  EXPECT_TRUE(is_unlocked(rows, mission_at(0)));
  EXPECT_FALSE(is_completed(rows, mission_at(0)))
      << "a refused request must not have written anything";
}

TEST_F(CampaignProgressionTest, AMissionFromAnotherCampaignIsRefused) {
  QString error;
  const auto advance = storage->complete_campaign_mission(
      QStringLiteral("some_other_campaign"), mission_at(0), &error);

  EXPECT_FALSE(advance.has_value());
  EXPECT_FALSE(is_completed(progress(), mission_at(0)));
}

TEST_F(CampaignProgressionTest, TheCampaignListReflectsProgressAsThePlayerSeesIt) {
  ASSERT_TRUE(complete(mission_at(0)).has_value());

  const QVariantList campaigns = storage->list_campaigns();
  ASSERT_FALSE(campaigns.isEmpty());

  QVariantMap entry;
  for (const QVariant& candidate : campaigns) {
    if (candidate.toMap().value(QStringLiteral("id")).toString() == campaign.id) {
      entry = candidate.toMap();
      break;
    }
  }
  ASSERT_FALSE(entry.isEmpty()) << "the shipped campaign is missing from the list";
  EXPECT_FALSE(entry.value(QStringLiteral("completed")).toBool());

  const QVariantList missions = entry.value(QStringLiteral("missions")).toList();
  ASSERT_EQ(static_cast<std::size_t>(missions.size()), campaign.missions.size());
  EXPECT_TRUE(is_completed(missions, mission_at(0)));
  EXPECT_TRUE(is_unlocked(missions, mission_at(1)));
  EXPECT_FALSE(is_unlocked(missions, mission_at(2)));
}

TEST_F(CampaignProgressionTest, ReimportingTheCampaignKeepsProgress) {
  // Re-reading the campaign file happens on every launch, and it must not reset
  // what the player has already cleared.
  ASSERT_TRUE(complete(mission_at(0)).has_value());
  ASSERT_TRUE(complete(mission_at(1)).has_value());

  QString error;
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(campaign, &error))
      << error.toStdString();

  const auto rows = progress();
  EXPECT_TRUE(is_completed(rows, mission_at(0)));
  EXPECT_TRUE(is_completed(rows, mission_at(1)));
  EXPECT_TRUE(is_unlocked(rows, mission_at(2)));
}

TEST_F(CampaignProgressionTest, ACampaignShortenedByAnUpdateStillAdvances) {
  // A mission removed from the middle of a campaign leaves a gap in the order
  // indices. Advancing by "order + 1" would strand the player on the gap.
  Game::Campaign::CampaignDefinition edited;
  edited.id = QStringLiteral("edited_campaign");
  edited.missions.push_back({QStringLiteral("first"), 0, {}, {}, {}, {}});
  edited.missions.push_back({QStringLiteral("third"), 2, {}, {}, {}, {}});

  QString error;
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(edited, &error))
      << error.toStdString();

  const auto advance =
      storage->complete_campaign_mission(edited.id, QStringLiteral("first"), &error);
  ASSERT_TRUE(advance.has_value()) << error.toStdString();
  EXPECT_EQ(advance->unlocked_mission_id, QStringLiteral("third"));
}

TEST(CampaignProgressionPersistenceTest, CompletionSurvivesReopeningTheDatabase) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("campaign.db"));
  const auto campaign = shipped_campaign();
  ASSERT_FALSE(campaign.missions.empty());

  {
    SaveStorage storage(path);
    QString error;
    ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();
    ASSERT_TRUE(storage.ensure_campaign_missions_in_db(campaign, &error))
        << error.toStdString();
    for (const auto& mission : campaign.missions) {
      ASSERT_TRUE(
          storage.complete_campaign_mission(campaign.id, mission.mission_id, &error)
              .has_value())
          << error.toStdString();
    }
  }

  SaveStorage reopened(path);
  QString error;
  ASSERT_TRUE(reopened.initialize(&error)) << error.toStdString();
  // The launch path re-imports the campaign file before reading progress.
  ASSERT_TRUE(reopened.ensure_campaign_missions_in_db(campaign, &error))
      << error.toStdString();

  EXPECT_TRUE(reopened.get_campaign_progress(campaign.id)
                  .value(QStringLiteral("completed"))
                  .toBool())
      << "a finished campaign forgot itself across a restart";

  const QVariantList rows = reopened.get_campaign_mission_progress(campaign.id);
  for (const auto& mission : campaign.missions) {
    EXPECT_TRUE(is_completed(rows, mission.mission_id))
        << mission.mission_id.toStdString();
    EXPECT_TRUE(is_unlocked(rows, mission.mission_id))
        << mission.mission_id.toStdString()
        << " must stay replayable after the campaign is finished";
  }
}

TEST_F(CampaignProgressionTest, AMissionDroppedByAnUpdateStopsBlockingCompletion) {
  // An older build shipped an extra mission; the update removes it. The row it
  // left in the progression tables must not keep the campaign unfinishable.
  Game::Campaign::CampaignDefinition old_version;
  old_version.id = QStringLiteral("shrinking_campaign");
  old_version.missions.push_back({QStringLiteral("one"), 0, {}, {}, {}, {}});
  old_version.missions.push_back({QStringLiteral("cut_later"), 1, {}, {}, {}, {}});
  old_version.missions.push_back({QStringLiteral("two"), 2, {}, {}, {}, {}});

  QString error;
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(old_version, &error))
      << error.toStdString();

  Game::Campaign::CampaignDefinition new_version;
  new_version.id = old_version.id;
  new_version.missions.push_back({QStringLiteral("one"), 0, {}, {}, {}, {}});
  new_version.missions.push_back({QStringLiteral("two"), 1, {}, {}, {}, {}});
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(new_version, &error))
      << error.toStdString();

  ASSERT_TRUE(
      storage->complete_campaign_mission(new_version.id, QStringLiteral("one"), &error)
          .has_value())
      << error.toStdString();
  const auto advance =
      storage->complete_campaign_mission(new_version.id, QStringLiteral("two"), &error);

  ASSERT_TRUE(advance.has_value()) << error.toStdString();
  EXPECT_TRUE(advance->campaign_completed)
      << "a mission removed by an update kept the campaign unfinishable";

  const QVariantList rows = storage->get_campaign_mission_progress(new_version.id);
  EXPECT_EQ(rows.size(), 2)
      << "the dropped mission is still listed in the player's progress";
}

// --- Failure injection -----------------------------------------------------
// The states a player can actually reach by quitting, crashing or updating at
// the wrong moment, and what progression has to do about them.

TEST_F(CampaignProgressionTest, ProgressIsDurableTheMomentTheVictoryLands) {
  // Quitting on the victory screen must not lose the win. The write has to be
  // committed by the time the call returns, not held in an open transaction.
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("progress.db"));

  SaveStorage writer(path);
  QString error;
  ASSERT_TRUE(writer.initialize(&error)) << error.toStdString();
  ASSERT_TRUE(writer.ensure_campaign_missions_in_db(campaign, &error))
      << error.toStdString();
  ASSERT_TRUE(
      writer.complete_campaign_mission(campaign.id, mission_at(0), &error).has_value())
      << error.toStdString();

  // A second connection, opened while the first is still alive, stands in for
  // the process that reads the save back after an abrupt exit.
  SaveStorage reader(path);
  ASSERT_TRUE(reader.initialize(&error)) << error.toStdString();
  const QVariantList rows = reader.get_campaign_mission_progress(campaign.id);
  EXPECT_TRUE(is_completed(rows, mission_at(0)))
      << "the win was not committed before the call returned";
  EXPECT_TRUE(is_unlocked(rows, mission_at(1)));
}

TEST_F(CampaignProgressionTest,
       ACampaignFlaggedCompleteWithoutTheMissionsIsNotBelieved) {
  // Exactly the state the old engine left behind: the campaign row says
  // finished after a single mission. What the player is shown is derived from
  // the missions, so it must still read as unfinished.
  QString error;
  ASSERT_TRUE(storage->mark_campaign_completed(campaign.id, &error))
      << error.toStdString();

  QVariantMap entry;
  for (const QVariant& candidate : storage->list_campaigns()) {
    if (candidate.toMap().value(QStringLiteral("id")).toString() == campaign.id) {
      entry = candidate.toMap();
    }
  }
  ASSERT_FALSE(entry.isEmpty());
  EXPECT_FALSE(entry.value(QStringLiteral("completed")).toBool())
      << "a stale campaign flag was taken at face value";

  // And the campaign can still be played to a real finish from there.
  for (const auto& mission : campaign.missions) {
    ASSERT_TRUE(complete(mission.mission_id).has_value());
  }
  for (const QVariant& candidate : storage->list_campaigns()) {
    if (candidate.toMap().value(QStringLiteral("id")).toString() == campaign.id) {
      EXPECT_TRUE(candidate.toMap().value(QStringLiteral("completed")).toBool());
    }
  }
}

TEST_F(CampaignProgressionTest, CompletingTheSameMissionRepeatedlyIsStable) {
  // A victory signal that arrives several times, or a transition that is
  // retried, must converge rather than accumulate.
  for (int attempt = 0; attempt < 5; ++attempt) {
    const auto advance = complete(mission_at(0));
    ASSERT_TRUE(advance.has_value()) << "attempt " << attempt;
    EXPECT_EQ(advance->newly_completed, attempt == 0);
    EXPECT_EQ(advance->unlocked_mission_id, mission_at(1));
    EXPECT_FALSE(advance->campaign_completed);
  }

  const auto rows = progress();
  EXPECT_EQ(static_cast<std::size_t>(rows.size()), campaign.missions.size());
  EXPECT_FALSE(is_unlocked(rows, mission_at(2)))
      << "repeated wins walked further than one mission";
}

TEST_F(CampaignProgressionTest, TheCompletionTimestampIsNotRewrittenByAReplay) {
  for (const auto& mission : campaign.missions) {
    ASSERT_TRUE(complete(mission.mission_id).has_value());
  }
  const QString first_completed_at = storage->get_campaign_progress(campaign.id)
                                         .value(QStringLiteral("completedAt"))
                                         .toString();
  ASSERT_FALSE(first_completed_at.isEmpty());

  ASSERT_TRUE(complete(mission_at(0)).has_value());

  EXPECT_EQ(storage->get_campaign_progress(campaign.id)
                .value(QStringLiteral("completedAt"))
                .toString(),
            first_completed_at)
      << "replaying a mission moved the date the campaign was won";
}

TEST_F(CampaignProgressionTest, AnEmptiedCampaignLeavesNoProgressBehind) {
  ASSERT_TRUE(complete(mission_at(0)).has_value());

  Game::Campaign::CampaignDefinition emptied;
  emptied.id = campaign.id;

  QString error;
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(emptied, &error))
      << error.toStdString();
  EXPECT_TRUE(storage->get_campaign_mission_progress(campaign.id).isEmpty());
}

} // namespace
