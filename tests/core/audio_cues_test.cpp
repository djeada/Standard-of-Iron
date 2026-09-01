#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

#include "app/audio/audio_resource_loader.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_settings.h"
#include "game/audio/cue_trace.h"

namespace {

auto load_catalog_ids() -> QSet<QString> {
  QFile file(QStringLiteral("assets/audio/audio_cues.json"));
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << "assets/audio/audio_cues.json";

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QJsonArray cues = document.object().value(QStringLiteral("cues")).toArray();

  QSet<QString> ids;
  for (const QJsonValue value : cues) {
    ids.insert(value.toObject().value(QStringLiteral("id")).toString());
  }
  return ids;
}

class AudioCueRegistryTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Audio::CueRegistry::instance().clear(); }
  void TearDown() override { Game::Audio::CueRegistry::instance().clear(); }
};

TEST(AudioCueCatalogTest, EveryCueConstantHasACatalogEntry) {
  const QSet<QString> catalog = load_catalog_ids();
  ASSERT_FALSE(catalog.isEmpty());

  for (const char* cue_id : Game::Audio::Cue::k_all) {
    EXPECT_TRUE(catalog.contains(QString::fromLatin1(cue_id)))
        << "cue constant with no catalog entry: " << cue_id;
  }
}

TEST(AudioCueCatalogTest, EveryCatalogEntryHasACueConstant) {
  QSet<QString> constants;
  for (const char* cue_id : Game::Audio::Cue::k_all) {
    constants.insert(QString::fromLatin1(cue_id));
  }

  for (const QString& cue_id : load_catalog_ids()) {
    EXPECT_TRUE(constants.contains(cue_id))
        << "catalog cue with no constant: " << cue_id.toStdString();
  }
}

TEST(AudioCueCatalogTest, EveryCueResolvesToAManifestTrackThatExists) {
  QFile file(QStringLiteral("assets/audio/audio_cues.json"));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const QJsonArray cues = QJsonDocument::fromJson(file.readAll())
                              .object()
                              .value(QStringLiteral("cues"))
                              .toArray();
  ASSERT_FALSE(cues.isEmpty());

  for (const QJsonValue value : cues) {
    const QJsonObject cue = value.toObject();
    const QString cue_id = cue.value(QStringLiteral("id")).toString();
    const QJsonArray resources = cue.value(QStringLiteral("resources")).toArray();
    EXPECT_FALSE(resources.isEmpty())
        << "cue bound to nothing: " << cue_id.toStdString();

    for (const QJsonValue resource : resources) {
      const QString resource_id = resource.toString();
      EXPECT_TRUE(AudioResourceLoader::has_manifest_entry(resource_id))
          << cue_id.toStdString() << " points at unknown resource "
          << resource_id.toStdString();
    }
  }

  EXPECT_TRUE(AudioResourceLoader::missing_asset_ids().isEmpty())
      << "manifest tracks whose file is missing: "
      << AudioResourceLoader::missing_asset_ids()
             .join(QStringLiteral(", "))
             .toStdString();
}

TEST(AudioCueCatalogTest, EveryMusicTrackIsReachableByTheTagsThatSelectMusic) {
  QFile file(QStringLiteral("assets/audio/audio_manifest.json"));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const QJsonArray tracks = QJsonDocument::fromJson(file.readAll())
                                .object()
                                .value(QStringLiteral("tracks"))
                                .toArray();
  ASSERT_FALSE(tracks.isEmpty());

  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::All);

  for (const QJsonValue value : tracks) {
    const QJsonObject track = value.toObject();
    if (track.value(QStringLiteral("category")).toString() != QStringLiteral("music")) {
      continue;
    }
    const QString track_id = track.value(QStringLiteral("id")).toString();
    const QJsonObject tags = track.value(QStringLiteral("tags")).toObject();

    QMap<QString, QString> query;
    for (const QString& key : {QStringLiteral("ambient_state"),
                               QStringLiteral("screen_context"),
                               QStringLiteral("event")}) {
      if (tags.contains(key)) {
        query.insert(key, tags.value(key).toString());
        break;
      }
    }
    ASSERT_FALSE(query.isEmpty())
        << track_id.toStdString()
        << " carries no ambient_state, screen_context or event tag, so no state "
           "and no screen can ever select it";

    if (query.contains(QStringLiteral("event"))) {
      continue;
    }
    EXPECT_TRUE(AudioResourceLoader::find_resource_ids(AudioCategory::MUSIC, query)
                    .contains(track_id))
        << track_id.toStdString()
        << " is not returned by the query the audio "
           "coordinator runs for its own tags";
  }
}

class ShippedAudioTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& audio = AudioSystem::get_instance();
    audio.shutdown();
    if (!audio.initialize()) {
      GTEST_SKIP() << "Audio backend is unavailable in this environment";
    }

    m_saved_master = audio.get_master_volume();
    m_saved_sound = audio.get_sound_volume();
    audio.set_master_volume(1.0F);
    audio.set_sound_volume(1.0F);

    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Startup);
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
    AudioResourceLoader::load_audio_cues();
  }

  void TearDown() override {
    Game::Audio::CueRegistry::instance().clear();
    auto& audio = AudioSystem::get_instance();
    audio.set_master_volume(m_saved_master);
    audio.set_sound_volume(m_saved_sound);
    audio.shutdown();
  }

private:
  float m_saved_master{1.0F};
  float m_saved_sound{1.0F};
};

namespace {

auto plays_audibly(const char* cue_id) -> bool {
  for (int attempt = 0; attempt < 60; ++attempt) {
    if (!Game::Audio::play_cue(cue_id)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    for (int settle = 0; settle < 50; ++settle) {
      if (AudioSystem::get_instance().get_active_channel_count() > 0) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  return false;
}

} // namespace

TEST_F(ShippedAudioTest, TheCommandersBowCuesReachTheMixer) {
  auto& audio = AudioSystem::get_instance();
  audio.set_master_volume(1.0F);
  audio.set_sound_volume(1.0F);

  for (const char* cue_id : {Game::Audio::Cue::k_combat_bow_draw,
                             Game::Audio::Cue::k_combat_bow_full_draw,
                             Game::Audio::Cue::k_combat_bow_strain,
                             Game::Audio::Cue::k_combat_bow_loose_heavy}) {
    EXPECT_TRUE(plays_audibly(cue_id))
        << cue_id << " never opened a channel: it is bound but not audible";
  }
}

TEST_F(ShippedAudioTest, TheCuesFiredHardestReachTheMixer) {
  auto& audio = AudioSystem::get_instance();
  audio.set_master_volume(1.0F);
  audio.set_sound_volume(1.0F);

  for (const char* cue_id : {Game::Audio::Cue::k_combat_hit_sword,
                             Game::Audio::Cue::k_combat_hit_spear,
                             Game::Audio::Cue::k_combat_hit_arrow,
                             Game::Audio::Cue::k_combat_hit_siege,
                             Game::Audio::Cue::k_combat_hit_generic,
                             Game::Audio::Cue::k_combat_death,
                             Game::Audio::Cue::k_move_footstep,
                             Game::Audio::Cue::k_move_footstep_hard,
                             Game::Audio::Cue::k_move_footstep_run,
                             Game::Audio::Cue::k_wildlife_wolf_hunt,
                             Game::Audio::Cue::k_wildlife_wolf_bite}) {
    EXPECT_TRUE(plays_audibly(cue_id))
        << cue_id << " never opened a channel: it is bound but not audible";
  }
}

TEST_F(ShippedAudioTest, EveryCueInTheCatalogueReachesTheMixer) {
  auto& audio = AudioSystem::get_instance();
  audio.set_master_volume(1.0F);
  audio.set_sound_volume(1.0F);

  std::vector<std::string> silent;
  for (const char* cue_id : Game::Audio::Cue::k_all) {
    if (!plays_audibly(cue_id)) {
      silent.emplace_back(cue_id);
    }
  }

  std::string joined;
  for (const auto& cue_id : silent) {
    joined += cue_id + " ";
  }
  EXPECT_TRUE(silent.empty()) << "bound but never audible, so silent in game: "
                              << joined;
}

namespace {

auto sword_pool() -> std::vector<std::string> {
  return {"sfx.combat.sword_hit_01",
          "sfx.combat.blade_clash_01",
          "sfx.combat.blade_clash_02",
          "sfx.combat.blade_clash_03"};
}

auto play_and_collect(const char* cue_id,
                      int times,
                      int pace_ms = 0) -> std::vector<std::string> {
  std::vector<std::string> chosen;
  for (int attempt = 0; attempt < times; ++attempt) {
    if (pace_ms > 0 && attempt > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(pace_ms));
    }
    if (!Game::Audio::play_cue(cue_id)) {
      continue;
    }
    chosen.push_back(Game::Audio::CueRegistry::instance().last_variant(cue_id));
  }
  return chosen;
}

} // namespace

TEST_F(ShippedAudioTest, ARepeatedCueWalksItsWholePoolInsteadOfFavouringOne) {
  Game::Audio::CueBinding binding;
  binding.resource_ids = sword_pool();
  for (const auto& resource_id : binding.resource_ids) {
    ASSERT_TRUE(AudioSystem::get_instance().has_resource(resource_id))
        << resource_id << " is not loaded, so this test cannot judge selection";
  }
  Game::Audio::CueRegistry::instance().bind("test.cue.pool", binding);
  Game::Audio::CueTrace::instance().reset();

  const std::vector<std::string> chosen = play_and_collect("test.cue.pool", 24);
  ASSERT_GE(chosen.size(), 20U);

  const QSet<QString> distinct = [&] {
    QSet<QString> ids;
    for (const auto& id : chosen) {
      ids.insert(QString::fromStdString(id));
    }
    return ids;
  }();
  EXPECT_EQ(distinct.size(), 4) << "part of the pool was never reached";

  const std::size_t window =
      Game::Audio::CueRegistry::shuffle_history_size(binding.resource_ids.size());
  ASSERT_EQ(window, 2U);
  for (std::size_t index = window; index < chosen.size(); ++index) {
    for (std::size_t back = 1; back <= window; ++back) {
      EXPECT_NE(chosen[index], chosen[index - back])
          << "variant " << chosen[index] << " repeated within " << window
          << " plays at position " << index;
    }
  }
}

TEST_F(ShippedAudioTest, AZeroWeightVariantIsNeverChosen) {
  Game::Audio::CueBinding binding;
  binding.resource_ids = {"sfx.combat.sword_hit_01", "sfx.combat.blade_clash_01"};
  binding.weights = {1.0F, 0.0F};
  Game::Audio::CueRegistry::instance().bind("test.cue.weighted", binding);
  Game::Audio::CueTrace::instance().reset();

  const std::vector<std::string> chosen = play_and_collect("test.cue.weighted", 12);
  ASSERT_FALSE(chosen.empty());

  for (const auto& id : chosen) {
    EXPECT_EQ(id, "sfx.combat.sword_hit_01")
        << "a variant weighted to zero was played anyway";
  }
}

TEST_F(ShippedAudioTest, VariantPlayCountsAreReportedForTuning) {
  Game::Audio::CueBinding binding;
  binding.resource_ids = sword_pool();
  Game::Audio::CueRegistry::instance().bind("test.cue.counted_pool", binding);
  Game::Audio::CueTrace::instance().reset();

  play_and_collect("test.cue.counted_pool", 8, 120);
  for (int settle = 0; settle < 100; ++settle) {
    if (Game::Audio::CueTrace::instance().record_for("test.cue.counted_pool").accepted >
        0U) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  const auto record =
      Game::Audio::CueTrace::instance().record_for("test.cue.counted_pool");
  EXPECT_FALSE(record.resource_plays.empty())
      << "the trace cannot say which variants a pool actually used";

  const auto pool = sword_pool();
  std::uint64_t total = 0;
  for (const auto& [resource, plays] : record.resource_plays) {
    EXPECT_NE(std::find(pool.begin(), pool.end(), resource), pool.end())
        << "the trace credited a play to " << resource << ", which is not in the pool";
    total += plays;
  }
  EXPECT_EQ(total, record.accepted)
      << "the per-variant counts do not add up to what was actually played";
}

TEST_F(ShippedAudioTest, ABurstOfImpactsKeepsSoundingInsteadOfGoingQuiet) {
  Game::Audio::CueBinding binding;
  binding.resource_ids = sword_pool();
  Game::Audio::CueRegistry::instance().bind("test.cue.burst", binding);
  Game::Audio::CueTrace::instance().reset();

  for (int strike = 0; strike < 4; ++strike) {
    Game::Audio::play_cue("test.cue.burst");
  }

  for (int settle = 0; settle < 100; ++settle) {
    if (Game::Audio::CueTrace::instance().record_for("test.cue.burst").requests >= 4U) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  const auto record = Game::Audio::CueTrace::instance().record_for("test.cue.burst");
  EXPECT_EQ(record.outcomes.at(
                static_cast<std::size_t>(Game::Audio::CueOutcome::ResourceCooldown)),
            0U)
      << "blows landed in a burst and the pool sent them to a variant that was still "
         "on cooldown while other variants were free";
  EXPECT_GE(record.accepted, 4U);
}

TEST(AudioVariantSelectionTest, OnlyPoolsBigEnoughToBenefitKeepAHistory) {
  EXPECT_EQ(Game::Audio::CueRegistry::shuffle_history_size(1), 0U)
      << "a single-asset cue has nothing to avoid repeating";
  EXPECT_EQ(Game::Audio::CueRegistry::shuffle_history_size(2), 1U);
  EXPECT_EQ(Game::Audio::CueRegistry::shuffle_history_size(3), 1U);
  EXPECT_EQ(Game::Audio::CueRegistry::shuffle_history_size(4), 2U)
      << "half the pool is the widest window that still leaves a real choice";
}

TEST_F(AudioCueRegistryTest, UnboundCueIsSilentInsteadOfFailing) {
  EXPECT_FALSE(Game::Audio::play_cue("cue.that.does.not.exist"));
  EXPECT_FALSE(
      Game::Audio::CueRegistry::instance().is_bound("cue.that.does.not.exist"));
}

TEST_F(AudioCueRegistryTest, CueBoundToNoResourcesIsSilent) {
  auto& registry = Game::Audio::CueRegistry::instance();
  registry.bind("test.cue.empty", Game::Audio::CueBinding{});

  EXPECT_FALSE(registry.is_bound("test.cue.empty"));
  EXPECT_FALSE(registry.play("test.cue.empty"));
}

TEST_F(AudioCueRegistryTest, CueBoundToUnloadedResourceIsSilent) {
  auto& registry = Game::Audio::CueRegistry::instance();
  Game::Audio::CueBinding binding;
  binding.resource_ids = {"resource.never.loaded"};
  registry.bind("test.cue.unloaded", binding);

  EXPECT_TRUE(registry.is_bound("test.cue.unloaded"));
  EXPECT_FALSE(registry.play("test.cue.unloaded"));
}

TEST_F(AudioCueRegistryTest, SilentCuesAreReportedForAuditing) {
  auto& registry = Game::Audio::CueRegistry::instance();
  registry.bind("test.cue.reported", Game::Audio::CueBinding{});
  registry.play("test.cue.reported");

  const auto silent = registry.silent_cues();
  EXPECT_NE(std::find(silent.begin(), silent.end(), "test.cue.reported"), silent.end());
}

TEST(AudioMixTest, InformationOutranksAtmosphere) {
  const auto defaults = Game::Audio::Settings::first_run_volumes();

  EXPECT_FLOAT_EQ(defaults.voice, 1.0F) << "a commander speaking is never background";
  EXPECT_FLOAT_EQ(defaults.sound, 1.0F) << "cues are how the game talks to the player";
  EXPECT_LT(defaults.music, defaults.sound) << "music must not compete with a cue";
  EXPECT_LT(defaults.ambience, defaults.music)
      << "ambience is texture and sits under everything that carries meaning";
  EXPECT_GT(defaults.master, 0.0F);
}

TEST(AudioMixTest, EachCategoryHasAPolyphonyCap) {
  EXPECT_LE(AudioConstants::MAX_CONCURRENT_VOICE, 2U)
      << "overlapping voice lines stop being intelligible";
  EXPECT_LT(AudioConstants::MAX_CONCURRENT_VOICE,
            AudioConstants::MAX_CONCURRENT_AMBIENCE);
  EXPECT_LT(AudioConstants::MAX_CONCURRENT_AMBIENCE,
            AudioConstants::MAX_CONCURRENT_SFX);
  EXPECT_LE(AudioConstants::MAX_CONCURRENT_SFX, AudioConstants::DEFAULT_MAX_CHANNELS)
      << "a single category may not exceed the whole mixer";
}

TEST(AudioProvenanceTest, EveryEffectDeclaresWhereItCameFrom) {
  QFile file(QStringLiteral("assets/audio/audio_manifest.json"));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << "assets/audio/audio_manifest.json";
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QJsonArray tracks = document.object().value(QStringLiteral("tracks")).toArray();
  ASSERT_FALSE(tracks.isEmpty());

  const QSet<QString> known = {
      QStringLiteral("synth"), QStringLiteral("field"), QStringLiteral("generated")};
  QStringList untagged;
  QStringList unknown;
  int effects = 0;

  for (const QJsonValue value : tracks) {
    const QJsonObject track = value.toObject();
    if (track.value(QStringLiteral("category")).toString() != QStringLiteral("sfx")) {
      continue;
    }
    ++effects;
    const QString path = track.value(QStringLiteral("path")).toString();
    const QString source = track.value(QStringLiteral("tags"))
                               .toObject()
                               .value(QStringLiteral("source"))
                               .toString();
    if (source.isEmpty()) {
      untagged.append(path);
    } else if (!known.contains(source)) {
      unknown.append(path + " -> " + source);
    }
  }

  EXPECT_GT(effects, 0);
  EXPECT_TRUE(untagged.isEmpty())
      << "effects with no source tag: " << untagged.join(", ").toStdString();
  EXPECT_TRUE(unknown.isEmpty()) << "effects with an unrecognised source tag: "
                                 << unknown.join(", ").toStdString();
}

} // namespace

namespace {

auto load_catalog_cues() -> QJsonArray {
  QFile file(QStringLiteral("assets/audio/audio_cues.json"));
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << "assets/audio/audio_cues.json";
  return QJsonDocument::fromJson(file.readAll())
      .object()
      .value(QStringLiteral("cues"))
      .toArray();
}

} // namespace

TEST(AudioImportanceTest, EveryCueDeclaresAKnownImportance) {
  const QJsonArray cues = load_catalog_cues();
  ASSERT_FALSE(cues.isEmpty());

  const QSet<QString> levels{QStringLiteral("required"),
                             QStringLiteral("optional"),
                             QStringLiteral("ambient")};

  for (const QJsonValue value : cues) {
    const QJsonObject cue = value.toObject();
    const QString importance = cue.value(QStringLiteral("importance")).toString();
    EXPECT_TRUE(levels.contains(importance))
        << cue.value(QStringLiteral("id")).toString().toStdString()
        << " declares importance \"" << importance.toStdString() << "\"";
  }
}

TEST(AudioPacingTest, RepeatingAnnouncementsCannotMachineGun) {
  const QJsonArray cues = load_catalog_cues();
  ASSERT_FALSE(cues.isEmpty());

  const QMap<QString, int> floor_ms = {
      {QStringLiteral("alert.base_under_attack"), 10000},
      {QStringLiteral("alert.unit_lost"), 4000},
      {QStringLiteral("alert.low_resources"), 3000},
      {QStringLiteral("alert.population_limit"), 4000},
  };

  int checked = 0;
  for (const QJsonValue value : cues) {
    const QJsonObject cue = value.toObject();
    const QString id = cue.value(QStringLiteral("id")).toString();
    const int cooldown = cue.value(QStringLiteral("cooldown_ms")).toInt();

    if (floor_ms.contains(id)) {
      ++checked;
      EXPECT_GE(cooldown, floor_ms.value(id))
          << id.toStdString() << " would repeat every " << cooldown
          << " ms while its situation lasts";
    }
    if (id.startsWith(QStringLiteral("alert."))) {
      EXPECT_GE(cooldown, 500)
          << id.toStdString() << " is an announcement with almost no cooldown";
    }
  }
  EXPECT_EQ(checked, floor_ms.size()) << "a paced cue was renamed or dropped";
}

TEST(AudioImportanceTest, EveryRequiredCueHasAPlayableBinding) {
  const QJsonArray cues = load_catalog_cues();
  ASSERT_FALSE(cues.isEmpty());

  int required = 0;
  for (const QJsonValue value : cues) {
    const QJsonObject cue = value.toObject();
    if (cue.value(QStringLiteral("importance")).toString() !=
        QStringLiteral("required")) {
      continue;
    }
    ++required;
    const QJsonArray resources = cue.value(QStringLiteral("resources")).toArray();
    EXPECT_FALSE(resources.isEmpty())
        << "required cue is silent: "
        << cue.value(QStringLiteral("id")).toString().toStdString();
  }

  EXPECT_GT(required, 0) << "no cue is classified as required any more";
}

TEST(AudioImportanceTest, TheCriticalPlayerFeedbackCuesAreRequired) {
  const QJsonArray cues = load_catalog_cues();
  ASSERT_FALSE(cues.isEmpty());

  QSet<QString> required;
  for (const QJsonValue value : cues) {
    const QJsonObject cue = value.toObject();
    if (cue.value(QStringLiteral("importance")).toString() ==
        QStringLiteral("required")) {
      required.insert(cue.value(QStringLiteral("id")).toString());
    }
  }

  for (const char* cue_id : {"command.accept",
                             "command.refuse",
                             "combat.block",
                             "combat.perfect_guard",
                             "combat.dodge",
                             "combat.death",
                             "alert.base_under_attack",
                             "state.commander_enter",
                             "state.commander_exit"}) {
    EXPECT_TRUE(required.contains(QString::fromLatin1(cue_id)))
        << cue_id << " lost its required classification";
  }
}

TEST(AudioProvenanceTest, NoTrackShipsWithUnknownRightsUnlessItIsOnTheBaseline) {
  QFile manifest_file(QStringLiteral("assets/audio/audio_manifest.json"));
  ASSERT_TRUE(manifest_file.open(QIODevice::ReadOnly));
  const QJsonArray tracks = QJsonDocument::fromJson(manifest_file.readAll())
                                .object()
                                .value(QStringLiteral("tracks"))
                                .toArray();
  ASSERT_FALSE(tracks.isEmpty());

  QFile baseline_file(QStringLiteral("assets/audio/audio_provenance_baseline.json"));
  ASSERT_TRUE(baseline_file.open(QIODevice::ReadOnly))
      << "the provenance baseline is missing, so nothing records which files "
         "still need their rights written down";
  QSet<QString> baseline;
  for (const QJsonValue value : QJsonDocument::fromJson(baseline_file.readAll())
                                    .object()
                                    .value(QStringLiteral("rights_not_yet_recorded"))
                                    .toArray()) {
    baseline.insert(value.toString());
  }

  QStringList unknown;
  int recorded = 0;
  for (const QJsonValue value : tracks) {
    const QJsonObject track = value.toObject();
    const QJsonObject provenance = track.value(QStringLiteral("provenance")).toObject();
    const bool stated =
        !provenance.value(QStringLiteral("origin")).toString().isEmpty() &&
        !provenance.value(QStringLiteral("licence")).toString().isEmpty();
    if (stated) {
      ++recorded;
      continue;
    }
    const QString id = track.value(QStringLiteral("id")).toString();
    if (!baseline.contains(id)) {
      unknown.append(id);
    }
  }

  EXPECT_GT(recorded, 0);
  EXPECT_TRUE(unknown.isEmpty()) << "audio shipping with unknown usage rights: "
                                 << unknown.join(QStringLiteral(", ")).toStdString();
}

TEST(AudioCueCatalogTest, DeclaredWeightsMatchTheirResourceList) {
  const QJsonArray cues = load_catalog_cues();
  ASSERT_FALSE(cues.isEmpty());

  for (const QJsonValue value : cues) {
    const QJsonObject cue = value.toObject();
    const QJsonArray weights = cue.value(QStringLiteral("weights")).toArray();
    if (weights.isEmpty()) {
      continue;
    }
    EXPECT_EQ(weights.size(), cue.value(QStringLiteral("resources")).toArray().size())
        << cue.value(QStringLiteral("id")).toString().toStdString()
        << " weights one set of variants and plays another";
    for (const QJsonValue weight : weights) {
      EXPECT_GE(weight.toDouble(-1.0), 0.0)
          << cue.value(QStringLiteral("id")).toString().toStdString()
          << " declares a negative weight";
    }
  }
}
