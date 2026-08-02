#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <gtest/gtest.h>
#include <string>

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

} // namespace
