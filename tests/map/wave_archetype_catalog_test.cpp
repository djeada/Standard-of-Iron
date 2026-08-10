#include <QCoreApplication>
#include <QDir>
#include <QTemporaryFile>

#include <gtest/gtest.h>

#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/wave_archetype_catalog.h"

using namespace Game::Mission;

namespace {

auto total_units(const std::vector<WaveComposition>& composition) -> int {
  int total = 0;
  for (const auto& entry : composition) {
    total += entry.count;
  }
  return total;
}

auto write_mission(const QString& body) -> QString {

  QTemporaryFile file(QDir::temp().filePath("soi_wave_XXXXXX.json"));
  file.setAutoRemove(false);
  if (!file.open()) {
    return {};
  }
  file.write(body.toUtf8());
  file.close();
  return file.fileName();
}

} // namespace

TEST(WaveArchetypeCatalog, BuiltInArchetypesExpandToRealSpawnTypes) {
  const auto& catalog = WaveArchetypeCatalog::instance();

  for (const auto& id : catalog.ids()) {
    const auto* archetype = catalog.find(id);
    ASSERT_NE(archetype, nullptr) << id.toStdString();
    EXPECT_FALSE(archetype->composition.empty()) << id.toStdString();
    for (const auto& entry : archetype->composition) {
      EXPECT_FALSE(entry.type.isEmpty()) << id.toStdString();
      EXPECT_GT(entry.count, 0) << id.toStdString();
    }
  }

  EXPECT_NE(catalog.find(QStringLiteral("assault")), nullptr);
  EXPECT_NE(catalog.find(QStringLiteral("ASSAULT")), nullptr);
  EXPECT_EQ(catalog.find(QStringLiteral("no_such_archetype")), nullptr);
}

TEST(WaveArchetypeCatalog, UnknownArchetypeExpandsToNothing) {
  EXPECT_TRUE(WaveArchetypeCatalog::instance()
                  .expand(QStringLiteral("no_such_archetype"), 1.0F)
                  .empty());
}

TEST(WaveArchetypeCatalog, StrengthScalesEveryEntryButNeverBelowOne) {
  const auto base =
      WaveArchetypeCatalog::instance().expand(QStringLiteral("assault"), 1.0F);
  const auto heavy =
      WaveArchetypeCatalog::instance().expand(QStringLiteral("assault"), 2.0F);
  const auto tiny =
      WaveArchetypeCatalog::instance().expand(QStringLiteral("assault"), 0.1F);

  ASSERT_EQ(base.size(), heavy.size());
  EXPECT_GT(total_units(heavy), total_units(base));
  for (const auto& entry : tiny) {
    EXPECT_GE(entry.count, 1);
  }
}

TEST(WaveArchetypeCatalog, DifficultyMultipliersRankFromEasyToVeryHard) {
  const float easy = difficulty_strength_multiplier(QStringLiteral("easy"));
  const float normal = difficulty_strength_multiplier(QStringLiteral("normal"));
  const float medium = difficulty_strength_multiplier(QStringLiteral("medium"));
  const float hard = difficulty_strength_multiplier(QStringLiteral("hard"));
  const float very_hard = difficulty_strength_multiplier(QStringLiteral("very_hard"));

  EXPECT_LT(easy, normal);
  EXPECT_FLOAT_EQ(normal, medium);
  EXPECT_LT(normal, hard);
  EXPECT_LT(hard, very_hard);

  EXPECT_FLOAT_EQ(difficulty_strength_multiplier(QString()), 1.0F);
  EXPECT_FLOAT_EQ(difficulty_strength_multiplier(QStringLiteral("nonsense")), 1.0F);
}

TEST(WaveArchetypeCatalog, ElitesAndTitlesSurviveScaling) {
  std::vector<WaveComposition> source;
  WaveComposition guard;
  guard.type = QStringLiteral("swordsman");
  guard.count = 4;
  guard.elite = true;
  guard.title = QStringLiteral("Consular guard");
  source.push_back(guard);

  const auto scaled = scale_wave_composition(source, 2.0F);

  ASSERT_EQ(scaled.size(), 1U);
  EXPECT_EQ(scaled[0].count, 8);
  EXPECT_TRUE(scaled[0].elite);
  EXPECT_EQ(scaled[0].title, QStringLiteral("Consular guard"));
}

TEST(MissionLoaderWaves, ParsesStateDrivenTriggerFields) {
  const QString path = write_mission(R"({
    "id": "wave_fields",
    "title": "Wave fields",
    "summary": "",
    "map_path": ":/assets/maps/map_forest.json",
    "ai_setups": [
      {
        "id": "roman_column",
        "nation": "roman_republic",
        "faction": "roman",
        "color": "red",
        "wave_escalation": 0.25,
        "waves": [
          {
            "timing": 60.0,
            "phase": 2,
            "label": "Second crossing",
            "trigger": "after_previous_cleared",
            "grace_seconds": 30.0,
            "warning_seconds": 12.0,
            "archetype": "assault",
            "strength": 1.5,
            "entry_points": [{"x": 10, "z": 20}, {"x": 30, "z": 40}],
            "clear_reward": {"gold": 150, "food": 75},
            "composition": [
              {"type": "swordsman", "count": 3, "elite": true, "title": "Guard"}
            ]
          }
        ]
      }
    ],
    "victory_conditions": [{"type": "destroy_all_enemies", "description": "win"}]
  })");
  ASSERT_FALSE(path.isEmpty());

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(path, mission, &error))
      << error.toStdString();
  ASSERT_EQ(mission.ai_setups.size(), 1U);

  const auto& ai = mission.ai_setups[0];
  EXPECT_FLOAT_EQ(ai.wave_escalation, 0.25F);
  ASSERT_EQ(ai.waves.size(), 1U);

  const auto& wave = ai.waves[0];
  EXPECT_EQ(wave.trigger, WaveTriggerMode::AfterPreviousCleared);
  EXPECT_FLOAT_EQ(wave.grace_seconds, 30.0F);
  EXPECT_FLOAT_EQ(wave.warning_seconds, 12.0F);
  EXPECT_FLOAT_EQ(wave.strength, 1.5F);
  EXPECT_EQ(wave.archetype, QStringLiteral("assault"));
  EXPECT_EQ(wave.label, QStringLiteral("Second crossing"));
  ASSERT_TRUE(wave.phase.has_value());
  EXPECT_EQ(*wave.phase, 2);
  EXPECT_EQ(wave.clear_reward.get(Game::Systems::ResourceType::Gold), 150);
  EXPECT_EQ(wave.clear_reward.get(Game::Systems::ResourceType::Food), 75);
  ASSERT_EQ(wave.entry_points.size(), 2U);
  EXPECT_EQ(wave.resolved_entry_points().size(), 2U);
  ASSERT_EQ(wave.composition.size(), 1U);
  EXPECT_TRUE(wave.composition[0].elite);
  EXPECT_EQ(wave.composition[0].title, QStringLiteral("Guard"));

  QFile::remove(path);
}

TEST(MissionLoaderWaves, OmittedTriggerFieldsFallBackToTheTimedSchedule) {
  const QString path = write_mission(R"({
    "id": "legacy_waves",
    "title": "Legacy waves",
    "summary": "",
    "map_path": ":/assets/maps/map_forest.json",
    "ai_setups": [
      {
        "id": "roman_column",
        "nation": "roman_republic",
        "faction": "roman",
        "color": "red",
        "waves": [
          {
            "timing": 120.0,
            "entry_point": {"x": 5, "z": 6},
            "composition": [{"type": "spearman", "count": 4}]
          }
        ]
      }
    ],
    "victory_conditions": [{"type": "destroy_all_enemies", "description": "win"}]
  })");
  ASSERT_FALSE(path.isEmpty());

  MissionDefinition mission;
  QString error;
  ASSERT_TRUE(MissionLoader::load_from_json_file(path, mission, &error))
      << error.toStdString();
  ASSERT_EQ(mission.ai_setups.size(), 1U);
  ASSERT_EQ(mission.ai_setups[0].waves.size(), 1U);

  const auto& wave = mission.ai_setups[0].waves[0];
  EXPECT_EQ(wave.trigger, WaveTriggerMode::Time);
  EXPECT_FLOAT_EQ(wave.grace_seconds, k_default_wave_grace_seconds);
  EXPECT_FLOAT_EQ(wave.warning_seconds, k_default_wave_warning_seconds);
  EXPECT_FLOAT_EQ(wave.strength, 1.0F);
  EXPECT_FALSE(wave.phase.has_value());
  EXPECT_TRUE(wave.archetype.isEmpty());
  EXPECT_TRUE(wave.clear_reward.empty());
  EXPECT_TRUE(wave.entry_points.empty());
  ASSERT_EQ(wave.resolved_entry_points().size(), 1U);
  EXPECT_FLOAT_EQ(wave.resolved_entry_points()[0].x, 5.0F);

  QFile::remove(path);
}
