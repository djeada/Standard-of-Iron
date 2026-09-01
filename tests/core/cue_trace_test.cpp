#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <string>

#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/cue_trace.h"
#include "game/audio/spatial.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"

namespace {

using Game::Audio::CueOutcome;
using Game::Audio::CueTrace;

class CueTraceTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Audio::CueRegistry::instance().clear();
    CueTrace::instance().reset();
  }

  void TearDown() override {
    Game::Audio::CueRegistry::instance().clear();
    CueTrace::instance().reset();
    qunsetenv("SOI_AUDIO_TRACE_SUMMARY");
  }
};

TEST_F(CueTraceTest, EveryOutcomeHasAName) {
  for (std::size_t index = 0; index < Game::Audio::k_cue_outcome_count; ++index) {
    const auto* name = Game::Audio::cue_outcome_name(static_cast<CueOutcome>(index));
    EXPECT_STRNE(name, "unknown") << "outcome " << index << " has no reported name";
  }
}

TEST_F(CueTraceTest, ARequestIsCountedOncePerOutcome) {
  auto& trace = CueTrace::instance();
  trace.record("test.cue.counted", "resource.one", CueOutcome::Accepted);
  trace.record("test.cue.counted", "resource.two", CueOutcome::ResourceCooldown);
  trace.record("test.cue.counted", "resource.two", CueOutcome::ResourceCooldown);

  const auto record = trace.record_for("test.cue.counted");
  EXPECT_EQ(record.requests, 3U);
  EXPECT_EQ(record.accepted, 1U);
  EXPECT_EQ(record.outcomes.at(static_cast<std::size_t>(CueOutcome::ResourceCooldown)),
            2U);
  EXPECT_EQ(record.last_resource_id, "resource.two");
}

TEST_F(CueTraceTest, AnUnnamedCueIsNotRecorded) {
  auto& trace = CueTrace::instance();
  trace.record("", "resource.one", CueOutcome::Accepted);

  EXPECT_TRUE(trace.records().empty());
}

TEST_F(CueTraceTest, CuesRequestedButNeverHeardAreReported) {
  auto& trace = CueTrace::instance();
  trace.record("test.cue.heard", "resource.one", CueOutcome::Accepted);
  trace.record("test.cue.silent", "resource.two", CueOutcome::ResourceNotLoaded);

  const auto silent = trace.never_accepted_cues();
  EXPECT_EQ(silent, std::vector<std::string>{"test.cue.silent"});
}

TEST_F(CueTraceTest, AnUnboundCueRequestReachesTheTrace) {
  EXPECT_FALSE(Game::Audio::play_cue("test.cue.unbound.trace"));

  const auto record = CueTrace::instance().record_for("test.cue.unbound.trace");
  EXPECT_EQ(record.requests, 1U);
  EXPECT_EQ(record.accepted, 0U);
  EXPECT_EQ(record.outcomes.at(static_cast<std::size_t>(CueOutcome::Unbound)), 1U);
}

TEST_F(CueTraceTest, ACueBoundToAnUnloadedResourceIsTracedAsSuch) {
  Game::Audio::CueBinding binding;
  binding.resource_ids = {"resource.never.loaded"};
  Game::Audio::CueRegistry::instance().bind("test.cue.unloaded.trace", binding);

  EXPECT_FALSE(Game::Audio::play_cue("test.cue.unloaded.trace"));

  const auto record = CueTrace::instance().record_for("test.cue.unloaded.trace");
  EXPECT_EQ(record.outcomes.at(static_cast<std::size_t>(CueOutcome::NoLoadedResource)),
            1U);
}

TEST_F(CueTraceTest, ACueRequestNamesTheCodeThatFiredIt) {
  EXPECT_FALSE(Game::Audio::play_cue("test.cue.sourced"));

  const auto record = CueTrace::instance().record_for("test.cue.sourced");
  ASSERT_EQ(record.sources.size(), 1U);
  EXPECT_EQ(record.sources.front().rfind("tests/core/cue_trace_test.cpp:", 0), 0U)
      << "the trace named " << record.sources.front()
      << " instead of this test's own call site";
}

TEST_F(CueTraceTest, TheSourcePathIsRelativeToTheRepositoryRoot) {
  const std::string source = Game::Audio::cue_source_of();

  EXPECT_EQ(source.rfind("tests/core/cue_trace_test.cpp:", 0), 0U)
      << "an absolute build-machine path leaked into the trace: " << source;
}

TEST_F(CueTraceTest, TwoCallSitesForOneCueAreBothReported) {
  EXPECT_FALSE(Game::Audio::play_cue_from("test.cue.two_sites", 1.0F, "game/a.cpp:1"));
  EXPECT_FALSE(Game::Audio::play_cue_from("test.cue.two_sites", 1.0F, "game/b.cpp:2"));
  EXPECT_FALSE(Game::Audio::play_cue_from("test.cue.two_sites", 1.0F, "game/a.cpp:1"));

  const auto record = CueTrace::instance().record_for("test.cue.two_sites");
  EXPECT_EQ(record.requests, 3U);
  EXPECT_EQ(record.sources, (std::vector<std::string>{"game/a.cpp:1", "game/b.cpp:2"}));
}

TEST_F(CueTraceTest, TheListenerContextIsCarriedIntoTheTrace) {
  CueTrace::instance().set_listener(
      {.x = 12.0F, .y = 30.0F, .z = -4.5F, .mode = "rts"});

  const auto listener = CueTrace::instance().listener();
  EXPECT_FLOAT_EQ(listener.x, 12.0F);
  EXPECT_FLOAT_EQ(listener.z, -4.5F);
  EXPECT_EQ(listener.mode, "rts");
}

namespace {

QStringList g_captured_lines;

void capture_message(QtMsgType, const QMessageLogContext&, const QString& message) {
  g_captured_lines.append(message);
}

auto capture_trace_lines(const std::function<void()>& action) -> QStringList {
  g_captured_lines.clear();
  CueTrace::set_logging_enabled(true);
  const QtMessageHandler previous = qInstallMessageHandler(capture_message);
  action();
  qInstallMessageHandler(previous);
  CueTrace::set_logging_enabled(false);
  return g_captured_lines;
}

} // namespace

TEST_F(CueTraceTest, TheLoggedLineCarriesEveryFieldItPromises) {
  CueTrace::instance().set_listener(
      {.x = -318.9F, .y = 69.6F, .z = -196.4F, .mode = "rts"});

  const QStringList lines = capture_trace_lines([] {
    CueTrace::instance().record(
        "combat.hit.sword", "sfx.combat.sword_hit_01", CueOutcome::Accepted, "a.cpp:7");
  });

  ASSERT_FALSE(lines.isEmpty());
  const QString& line = lines.last();
  EXPECT_TRUE(line.contains(QStringLiteral("rts -318.9,69.6,-196.4")))
      << line.toStdString();
  EXPECT_TRUE(line.contains(QStringLiteral(
      "combat.hit.sword -> sfx.combat.sword_hit_01: accepted (a.cpp:7)")))
      << line.toStdString();
  EXPECT_FALSE(line.contains(QLatin1Char('%')))
      << "an unfilled place marker survived into the log: " << line.toStdString();
}

TEST_F(CueTraceTest, ADropWithNoResourceStillLogsTheCueAndTheReason) {
  const QStringList lines = capture_trace_lines([] {
    CueTrace::instance().record(
        "build.gate_open", {}, CueOutcome::AudienceFiltered, {});
  });

  ASSERT_FALSE(lines.isEmpty());
  EXPECT_TRUE(lines.last().contains(
      QStringLiteral("build.gate_open -> -: audience_filtered (-)")))
      << lines.last().toStdString();
}

TEST_F(CueTraceTest, ACueSilencedByTheAudienceFilterIsReportedNotLost) {
  Engine::Core::World world;
  Game::Audio::AudioEventHandler handler(&world);
  ASSERT_TRUE(handler.initialize());
  handler.set_local_owner_id(1);

  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent::for_owner(2, "test.cue.someone_elses"));

  const auto record = CueTrace::instance().record_for("test.cue.someone_elses");
  EXPECT_EQ(record.requests, 1U);
  EXPECT_EQ(record.outcomes.at(static_cast<std::size_t>(CueOutcome::AudienceFiltered)),
            1U)
      << "a cue dropped for not being the local player's business left no trace";
  ASSERT_EQ(record.sources.size(), 1U);
  EXPECT_EQ(record.sources.front().rfind("tests/core/cue_trace_test.cpp:", 0), 0U)
      << "an event-borne cue reported " << record.sources.front()
      << " instead of where it was published";

  handler.shutdown();
}

TEST(AudioSpatialTest, ASoundAtTheListenerIsUnchanged) {
  const Game::Audio::AudioListener listener{.position = {10.0F, 20.0F, 30.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true};

  const auto gain = Game::Audio::spatialize(listener, {10.0F, 20.0F, 30.0F});

  EXPECT_FLOAT_EQ(gain.volume_scale, 1.0F);
  EXPECT_FLOAT_EQ(gain.pan, 0.0F);
}

TEST(AudioSpatialTest, DistanceQuietensAndDistanceEnoughSilences) {
  const Game::Audio::AudioListener listener{
      .position = {0.0F, 0.0F, 0.0F}, .right_x = 1.0F, .right_z = 0.0F, .valid = true};

  const auto near_gain = Game::Audio::spatialize(listener, {0.0F, 0.0F, 10.0F});
  const auto middle = Game::Audio::spatialize(listener, {0.0F, 0.0F, 45.0F});
  const auto far_gain = Game::Audio::spatialize(listener, {0.0F, 0.0F, 200.0F});

  EXPECT_FLOAT_EQ(near_gain.volume_scale, 1.0F)
      << "a blow at the camera's feet should not be attenuated";
  EXPECT_LT(middle.volume_scale, 1.0F);
  EXPECT_GT(middle.volume_scale, 0.0F);
  EXPECT_FLOAT_EQ(far_gain.volume_scale, 0.0F)
      << "a fight on the far side of the map should not be audible at all";
}

TEST(AudioSpatialTest, TheSideASoundIsOnFollowsTheCamera) {
  const Game::Audio::AudioListener listener{
      .position = {0.0F, 0.0F, 0.0F}, .right_x = 1.0F, .right_z = 0.0F, .valid = true};

  EXPECT_GT(Game::Audio::spatialize(listener, {20.0F, 0.0F, 0.0F}).pan, 0.0F);
  EXPECT_LT(Game::Audio::spatialize(listener, {-20.0F, 0.0F, 0.0F}).pan, 0.0F);

  const Game::Audio::AudioListener turned{
      .position = {0.0F, 0.0F, 0.0F}, .right_x = -1.0F, .right_z = 0.0F, .valid = true};
  EXPECT_LT(Game::Audio::spatialize(turned, {20.0F, 0.0F, 0.0F}).pan, 0.0F)
      << "turning the camera around must swap which ear hears the fight";
}

TEST(AudioSpatialTest, PanningNeverBoostsAChannelAboveUnity) {
  for (float pan = -1.0F; pan <= 1.0F; pan += 0.1F) {
    const auto gains = Game::Audio::pan_gains(pan);
    EXPECT_LE(gains.first, 1.0F);
    EXPECT_LE(gains.second, 1.0F);
    EXPECT_GE(gains.first, 0.0F);
    EXPECT_GE(gains.second, 0.0F);
  }

  const auto centre = Game::Audio::pan_gains(0.0F);
  EXPECT_FLOAT_EQ(centre.first, 1.0F);
  EXPECT_FLOAT_EQ(centre.second, 1.0F);
}

TEST(AudioSpatialTest, WithoutAListenerNothingIsMoved) {
  const Game::Audio::AudioListener none;

  const auto gain = Game::Audio::spatialize(none, {500.0F, 0.0F, 500.0F});

  EXPECT_FLOAT_EQ(gain.volume_scale, 1.0F)
      << "before a camera exists, a cue must still be heard at full volume";
  EXPECT_FLOAT_EQ(gain.pan, 0.0F);
}

TEST_F(CueTraceTest, TheStatusOverlayShowsTheLastRequestAndWhatItDid) {
  CueTrace::instance().record("combat.hit.sword",
                              "sfx.combat.sword_hit_01",
                              CueOutcome::Accepted,
                              "game/audio/audio_event_handler.cpp:550");

  const QString overlay = QString::fromStdString(Game::Audio::format_status_overlay());

  EXPECT_TRUE(overlay.contains(QStringLiteral("combat.hit.sword")))
      << overlay.toStdString();
  EXPECT_TRUE(overlay.contains(QStringLiteral("sfx.combat.sword_hit_01")));
  EXPECT_TRUE(overlay.contains(QStringLiteral("accepted")));
  EXPECT_TRUE(overlay.contains(QStringLiteral("audio_event_handler.cpp:550")))
      << "the overlay hides which code asked for the sound";
  EXPECT_TRUE(overlay.contains(QStringLiteral("channels")))
      << "the overlay never states how many voices are live";
}

TEST_F(CueTraceTest, TheStatusOverlayNamesCuesThatAreMostlySilent) {
  for (int attempt = 0; attempt < 8; ++attempt) {
    CueTrace::instance().record(
        "build.gate_open", {}, CueOutcome::AudienceFiltered, "gate.cpp:1");
  }
  CueTrace::instance().record(
      "ui.click", "ui.click.synth", CueOutcome::Accepted, "qml");

  const auto struggling = CueTrace::instance().struggling_cues();
  ASSERT_EQ(struggling.size(), 1U);
  EXPECT_EQ(struggling.front().cue_id, "build.gate_open");

  const QString overlay = QString::fromStdString(Game::Audio::format_status_overlay());
  EXPECT_TRUE(
      overlay.contains(QStringLiteral("build.gate_open  0/8  audience_filtered")))
      << overlay.toStdString();
  EXPECT_FALSE(overlay.contains(QStringLiteral("ui.click  ")))
      << "a cue that is being heard was listed as a problem";
}

TEST_F(CueTraceTest, TheMissionSummaryReportsRequestsAndAudiblePlays) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  auto& trace = CueTrace::instance();
  trace.record("test.cue.summary", "resource.one", CueOutcome::Accepted);
  trace.record("test.cue.summary", "resource.one", CueOutcome::Muted);
  trace.record("test.cue.absent", "resource.two", CueOutcome::Unbound);

  const QString path = directory.filePath(QStringLiteral("audio_summary.json"));
  ASSERT_TRUE(trace.write_summary(path.toStdString(), "maps/test.json"));

  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

  EXPECT_EQ(root.value(QStringLiteral("label")).toString(),
            QStringLiteral("maps/test.json"));
  EXPECT_EQ(root.value(QStringLiteral("totals"))
                .toObject()
                .value(QStringLiteral("requests"))
                .toInt(),
            3);
  EXPECT_EQ(root.value(QStringLiteral("totals"))
                .toObject()
                .value(QStringLiteral("accepted"))
                .toInt(),
            1);

  const QJsonArray never_accepted =
      root.value(QStringLiteral("never_accepted")).toArray();
  ASSERT_EQ(never_accepted.size(), 1);
  EXPECT_EQ(never_accepted.at(0).toString(), QStringLiteral("test.cue.absent"));

  const QJsonArray cues = root.value(QStringLiteral("cues")).toArray();
  ASSERT_EQ(cues.size(), 2);
  const QJsonObject summary = cues.at(1).toObject();
  EXPECT_EQ(summary.value(QStringLiteral("cue")).toString(),
            QStringLiteral("test.cue.summary"));
  EXPECT_EQ(summary.value(QStringLiteral("requests")).toInt(), 2);
  EXPECT_EQ(summary.value(QStringLiteral("accepted")).toInt(), 1);
  EXPECT_EQ(summary.value(QStringLiteral("drops"))
                .toObject()
                .value(QStringLiteral("muted"))
                .toInt(),
            1);
}

TEST_F(CueTraceTest, TheSummaryIsOnlyExportedWhenAPathIsRequested) {
  auto& trace = CueTrace::instance();
  trace.record("test.cue.export", "resource.one", CueOutcome::Accepted);

  EXPECT_FALSE(trace.write_requested_summary("no/path/configured"));
  EXPECT_EQ(trace.record_for("test.cue.export").requests, 1U);
}

TEST_F(CueTraceTest, AnExportedSummaryStartsTheNextMissionFromZero) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("mission.json"));
  qputenv("SOI_AUDIO_TRACE_SUMMARY", path.toUtf8());

  auto& trace = CueTrace::instance();
  trace.record("test.cue.first_mission", "resource.one", CueOutcome::Accepted);
  ASSERT_TRUE(trace.write_requested_summary("maps/first.json"));

  EXPECT_TRUE(QFile::exists(path));
  EXPECT_TRUE(trace.records().empty());
  EXPECT_FALSE(trace.write_requested_summary("maps/first.json"));

  trace.record("test.cue.second_mission", "resource.one", CueOutcome::Accepted);
  ASSERT_TRUE(trace.write_requested_summary("maps/second.json"));

  const QString second = directory.filePath(QStringLiteral("mission.2.json"));
  ASSERT_TRUE(QFile::exists(second))
      << "a second mission overwrote the first mission's summary";

  QFile file(second);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
  EXPECT_EQ(root.value(QStringLiteral("label")).toString(),
            QStringLiteral("maps/second.json"));
  EXPECT_EQ(root.value(QStringLiteral("cues")).toArray().size(), 1);
}

} // namespace
