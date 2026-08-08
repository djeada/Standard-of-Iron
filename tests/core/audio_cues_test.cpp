#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <thread>

#include "app/core/audio_resource_loader.h"
#include "game/audio/audio_cues.h"

namespace {

auto load_catalog_ids() -> QSet<QString> {
  QFile file(QStringLiteral("assets/audio/audio_cues.json"));
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << "assets/audio/audio_cues.json";

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QJsonArray cues = document.object().value(QStringLiteral("cues")).toArray();

  QSet<QString> ids;
  for (const QJsonValue& value : cues) {
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

  for (const QJsonValue& value : cues) {
    const QJsonObject cue = value.toObject();
    const QString cue_id = cue.value(QStringLiteral("id")).toString();
    const QJsonArray resources = cue.value(QStringLiteral("resources")).toArray();
    EXPECT_FALSE(resources.isEmpty())
        << "cue bound to nothing: " << cue_id.toStdString();

    for (const QJsonValue& resource : resources) {
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

TEST(AudioProvenanceTest, EveryEffectDeclaresWhereItCameFrom) {
  QFile file(QStringLiteral("assets/audio/audio_manifest.json"));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << "assets/audio/audio_manifest.json";
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QJsonArray tracks = document.object().value(QStringLiteral("tracks")).toArray();
  ASSERT_FALSE(tracks.isEmpty());

  const QSet<QString> known = {QStringLiteral("synth"), QStringLiteral("field")};
  QStringList untagged;
  QStringList unknown;
  int effects = 0;

  for (const QJsonValue& value : tracks) {
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
