#include <QJsonObject>

#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_wave_director.h"
#include "game/mission/mission_waves.h"
#include "game/session/session_context.h"

namespace {

using Game::Mission::MissionWaveDirector;
using Game::Mission::PendingMissionWave;

auto make_wave(const QString& ai_id,
               int phase,
               float timing,
               Game::Mission::WaveTriggerMode trigger =
                   Game::Mission::WaveTriggerMode::Time) -> PendingMissionWave {
  PendingMissionWave wave;
  wave.ai_id = ai_id;
  wave.owner_id = 2;
  wave.phase_index = phase;
  wave.trigger_time = timing;
  wave.trigger = trigger;
  wave.warning_seconds = 10.0F;
  wave.grace_seconds = 20.0F;

  Game::Mission::WaveComposition composition;
  composition.type = QStringLiteral("spearman");
  composition.count = 4;
  wave.composition.push_back(composition);
  return wave;
}

void set_phase_count(std::vector<PendingMissionWave>& waves, int phase_count) {
  for (auto& wave : waves) {
    wave.phase_count = phase_count;
  }
}

auto spawn_unit(Engine::Core::World& world) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;
  unit->owner_id = 2;
  return entity->get_id();
}

void kill(Engine::Core::World& world, Engine::Core::EntityID entity_id) {
  auto* entity = world.get_entity(entity_id);
  ASSERT_NE(entity, nullptr);
  entity->get_component<Engine::Core::UnitComponent>()->health = 0;
}

void rout(Engine::Core::World& world, Engine::Core::EntityID entity_id) {
  auto* entity = world.get_entity(entity_id);
  ASSERT_NE(entity, nullptr);
  auto* morale = entity->add_component<Engine::Core::MoraleComponent>();
  morale->morale = 5.0F;
  morale->routing = true;
}

auto spawn_into(MissionWaveDirector& director,
                Engine::Core::World& world,
                std::size_t index,
                int count) -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> ids;
  ids.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    ids.push_back(spawn_unit(world));
  }
  director.note_spawned(index, ids);
  return ids;
}

} // namespace

TEST(MissionWaveDirectorTest, TimedWavesFireOnceTheClockPassesTheirTrigger) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{make_wave("roman", 1, 60.0F)};
  set_phase_count(waves, 1);

  MissionWaveDirector director;
  director.bind(&waves, &world);

  director.set_elapsed(10.0F);
  EXPECT_TRUE(director.advance().waves_to_spawn.empty());

  director.set_elapsed(60.0F);
  const auto effects = director.advance();
  ASSERT_EQ(effects.waves_to_spawn.size(), 1U);
  EXPECT_EQ(effects.waves_to_spawn[0], 0U);
}

TEST(MissionWaveDirectorTest, WarningFiresBeforeTheWaveLands) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{make_wave("roman", 1, 60.0F)};
  set_phase_count(waves, 1);

  MissionWaveDirector director;
  director.bind(&waves, &world);

  director.set_elapsed(49.0F);
  EXPECT_TRUE(director.advance().announcements.isEmpty());

  director.set_elapsed(51.0F);
  const auto warned = director.advance();
  EXPECT_EQ(warned.announcements.size(), 1);
  EXPECT_EQ(warned.audio_cues.size(), 1);
  EXPECT_TRUE(warned.waves_to_spawn.empty());
  EXPECT_TRUE(director.status()["warning"].toBool());

  director.set_elapsed(55.0F);
  EXPECT_TRUE(director.advance().announcements.isEmpty());
}

TEST(MissionWaveDirectorTest, AfterPreviousClearedWaitsForTheClearPlusGrace) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{
      make_wave("roman", 1, 10.0F),
      make_wave(
          "roman", 2, 20.0F, Game::Mission::WaveTriggerMode::AfterPreviousCleared)};
  waves[1].grace_seconds = 30.0F;
  waves[1].warning_seconds = 0.0F;
  set_phase_count(waves, 2);

  MissionWaveDirector director;
  director.bind(&waves, &world);

  director.set_elapsed(10.0F);
  ASSERT_EQ(director.advance().waves_to_spawn.size(), 1U);
  const auto first_wave_units = spawn_into(director, world, 0, 4);

  director.set_elapsed(200.0F);
  EXPECT_TRUE(director.advance().waves_to_spawn.empty());
  EXPECT_EQ(director.cleared_wave_count(), 0);

  for (const auto entity_id : first_wave_units) {
    kill(world, entity_id);
  }
  director.set_elapsed(210.0F);
  const auto cleared = director.advance();
  EXPECT_EQ(director.cleared_wave_count(), 1);
  EXPECT_TRUE(cleared.waves_to_spawn.empty()) << "grace period should still be running";

  director.set_elapsed(239.0F);
  EXPECT_TRUE(director.advance().waves_to_spawn.empty());

  director.set_elapsed(241.0F);
  EXPECT_EQ(director.advance().waves_to_spawn.size(), 1U);
}

TEST(MissionWaveDirectorTest, ClearingEarlyPullsTheNextWaveForward) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{
      make_wave("roman", 1, 10.0F),
      make_wave(
          "roman", 2, 20.0F, Game::Mission::WaveTriggerMode::AfterPreviousCleared)};
  waves[1].grace_seconds = 5.0F;
  waves[1].warning_seconds = 0.0F;
  set_phase_count(waves, 2);

  MissionWaveDirector director;
  director.bind(&waves, &world);

  director.set_elapsed(10.0F);
  ASSERT_EQ(director.advance().waves_to_spawn.size(), 1U);
  const auto units = spawn_into(director, world, 0, 4);
  for (const auto entity_id : units) {
    kill(world, entity_id);
  }

  director.set_elapsed(12.0F);
  (void)director.advance();
  director.set_elapsed(18.0F);

  EXPECT_EQ(director.advance().waves_to_spawn.size(), 1U);
}

TEST(MissionWaveDirectorTest, RoutedSurvivorsDoNotHoldAWaveOpen) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{make_wave("roman", 1, 0.0F)};
  set_phase_count(waves, 1);

  MissionWaveDirector director;
  director.bind(&waves, &world);
  director.set_elapsed(0.0F);
  ASSERT_EQ(director.advance().waves_to_spawn.size(), 1U);

  const auto units = spawn_into(director, world, 0, 4);
  kill(world, units[0]);
  kill(world, units[1]);

  director.set_elapsed(5.0F);
  (void)director.advance();
  EXPECT_EQ(director.cleared_wave_count(), 0);

  rout(world, units[2]);
  rout(world, units[3]);
  director.set_elapsed(6.0F);
  (void)director.advance();
  EXPECT_EQ(director.cleared_wave_count(), 1);
}

TEST(MissionWaveDirectorTest, ClearingAPhasePaysItsRewardExactlyOnce) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{make_wave("roman", 1, 0.0F)};
  set_phase_count(waves, 1);
  waves[0].clear_reward.set(Game::Systems::ResourceType::Gold, 200);

  MissionWaveDirector director;
  director.bind(&waves, &world);
  director.set_elapsed(0.0F);
  ASSERT_EQ(director.advance().waves_to_spawn.size(), 1U);

  const auto units = spawn_into(director, world, 0, 4);
  for (const auto entity_id : units) {
    kill(world, entity_id);
  }

  director.set_elapsed(5.0F);
  const auto paid = director.advance();
  EXPECT_EQ(paid.reward.get(Game::Systems::ResourceType::Gold), 200);
  EXPECT_TRUE(paid.all_cleared);

  director.set_elapsed(6.0F);
  const auto again = director.advance();
  EXPECT_EQ(again.reward.get(Game::Systems::ResourceType::Gold), 0);
  EXPECT_FALSE(again.all_cleared);
}

TEST(MissionWaveDirectorTest, PhaseCountsSpanEveryAiInThePhase) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{make_wave("north", 1, 0.0F),
                                        make_wave("south", 1, 0.0F)};
  set_phase_count(waves, 1);

  MissionWaveDirector director;
  director.bind(&waves, &world);
  director.set_elapsed(0.0F);
  ASSERT_EQ(director.advance().waves_to_spawn.size(), 2U);

  const auto north = spawn_into(director, world, 0, 2);
  const auto south = spawn_into(director, world, 1, 2);

  EXPECT_EQ(director.total_wave_count(), 1);

  for (const auto entity_id : north) {
    kill(world, entity_id);
  }
  director.set_elapsed(5.0F);
  (void)director.advance();
  EXPECT_EQ(director.cleared_wave_count(), 0)
      << "one AI's column dying is not the whole phase";

  for (const auto entity_id : south) {
    kill(world, entity_id);
  }
  director.set_elapsed(6.0F);
  (void)director.advance();
  EXPECT_EQ(director.cleared_wave_count(), 1);
}

TEST(MissionWaveDirectorTest, SavedStateSurvivesARoundTrip) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{
      make_wave("roman", 1, 10.0F),
      make_wave(
          "roman", 2, 20.0F, Game::Mission::WaveTriggerMode::AfterPreviousCleared)};
  set_phase_count(waves, 2);

  MissionWaveDirector director;
  director.bind(&waves, &world);
  director.set_elapsed(10.0F);
  ASSERT_EQ(director.advance().waves_to_spawn.size(), 1U);
  const auto units = spawn_into(director, world, 0, 3);
  kill(world, units[0]);

  director.set_elapsed(42.0F);
  (void)director.advance();
  const QJsonObject saved = director.serialize();

  std::vector<PendingMissionWave> restored_waves{
      make_wave("roman", 1, 10.0F),
      make_wave(
          "roman", 2, 20.0F, Game::Mission::WaveTriggerMode::AfterPreviousCleared)};
  set_phase_count(restored_waves, 2);

  MissionWaveDirector restored;
  restored.bind(&restored_waves, &world);
  restored.restore(saved);

  EXPECT_FLOAT_EQ(restored.elapsed(), 42.0F);
  EXPECT_TRUE(restored_waves[0].spawned);
  EXPECT_FALSE(restored_waves[1].spawned);
  EXPECT_EQ(restored_waves[0].spawned_entity_ids.size(), 3U);

  restored.set_elapsed(43.0F);
  EXPECT_TRUE(restored.advance().waves_to_spawn.empty());
}

TEST(MissionWaveDirectorTest, RestoringAMismatchedLayoutLeavesTheAuthoredSchedule) {
  Engine::Core::World world;
  std::vector<PendingMissionWave> waves{make_wave("roman", 1, 10.0F)};
  set_phase_count(waves, 1);

  MissionWaveDirector director;
  director.bind(&waves, &world);
  director.set_elapsed(10.0F);
  (void)director.advance();
  spawn_into(director, world, 0, 2);
  const QJsonObject saved = director.serialize();

  std::vector<PendingMissionWave> other_waves{make_wave("carthage", 1, 10.0F)};
  set_phase_count(other_waves, 1);

  MissionWaveDirector restored;
  restored.bind(&other_waves, &world);
  restored.restore(saved);

  EXPECT_FALSE(other_waves[0].spawned);
}

TEST(MissionWaveBuild, AuthoredPhasesGroupWavesAcrossAis) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::WaveComposition composition;
  composition.type = QStringLiteral("spearman");
  composition.count = 4;

  for (const char* id : {"north", "south"}) {
    Game::Mission::AISetup ai;
    ai.id = QString::fromLatin1(id);
    ai.nation = QStringLiteral("roman_republic");
    for (int phase = 1; phase <= 2; ++phase) {
      Game::Mission::Wave wave;

      wave.timing = 100.0F * static_cast<float>(phase) + (id[0] == 'n' ? 0.0F : 7.0F);
      wave.phase = phase;
      wave.composition.push_back(composition);
      ai.waves.push_back(wave);
    }
    mission.ai_setups.push_back(ai);
  }

  Game::Systems::LevelSnapshot level;
  const auto waves = Game::Mission::build_pending_mission_waves(
      {.mission = mission,
       .mission_difficulty = QStringLiteral("normal"),
       .level = level,
       .nations = Game::Session::SessionContext::active().nations()});

  ASSERT_EQ(waves.size(), 4U);
  for (const auto& wave : waves) {
    EXPECT_EQ(wave.phase_count, 2);
  }
  EXPECT_EQ(waves[0].phase_index, 1);
  EXPECT_EQ(waves[1].phase_index, 2);
  EXPECT_EQ(waves[2].phase_index, 1);
  EXPECT_EQ(waves[3].phase_index, 2);
  EXPECT_EQ(waves[0].owner_id, 2);
  EXPECT_EQ(waves[2].owner_id, 3);
}

TEST(MissionWaveBuild, DifficultyAndEscalationScaleWaveSize) {
  Game::Mission::WaveComposition composition;
  composition.type = QStringLiteral("spearman");
  composition.count = 10;

  auto build = [&composition](const QString& mission_difficulty,
                              const QString& ai_difficulty,
                              float escalation) {
    Game::Mission::MissionDefinition mission;
    Game::Mission::AISetup ai;
    ai.id = QStringLiteral("roman");
    ai.nation = QStringLiteral("roman_republic");
    ai.difficulty = ai_difficulty;
    ai.wave_escalation = escalation;
    for (int i = 0; i < 2; ++i) {
      Game::Mission::Wave wave;
      wave.timing = 60.0F * static_cast<float>(i + 1);
      wave.composition.push_back(composition);
      ai.waves.push_back(wave);
    }
    mission.ai_setups.push_back(ai);

    Game::Systems::LevelSnapshot level;
    return Game::Mission::build_pending_mission_waves(
        {.mission = mission,
         .mission_difficulty = mission_difficulty,
         .level = level,
         .nations = Game::Session::SessionContext::active().nations()});
  };

  const auto baseline = build(QStringLiteral("normal"), QStringLiteral("normal"), 0.0F);
  ASSERT_EQ(baseline.size(), 2U);
  EXPECT_EQ(Game::Mission::wave_unit_total(baseline[0]), 10);
  EXPECT_EQ(Game::Mission::wave_unit_total(baseline[1]), 10);

  const auto harder = build(QStringLiteral("normal"), QStringLiteral("hard"), 0.0F);
  EXPECT_GT(Game::Mission::wave_unit_total(harder[0]),
            Game::Mission::wave_unit_total(baseline[0]));

  const auto easier = build(QStringLiteral("easy"), QStringLiteral("normal"), 0.0F);
  EXPECT_LT(Game::Mission::wave_unit_total(easier[0]),
            Game::Mission::wave_unit_total(baseline[0]));

  const auto escalating =
      build(QStringLiteral("normal"), QStringLiteral("normal"), 0.5F);
  EXPECT_EQ(Game::Mission::wave_unit_total(escalating[0]), 10);
  EXPECT_GT(Game::Mission::wave_unit_total(escalating[1]),
            Game::Mission::wave_unit_total(escalating[0]));
}

TEST(MissionWaveBuild, ArchetypesExpandOnTopOfTheAuthoredComposition) {
  Game::Mission::MissionDefinition mission;
  Game::Mission::AISetup ai;
  ai.id = QStringLiteral("roman");
  ai.nation = QStringLiteral("roman_republic");

  Game::Mission::Wave wave;
  wave.timing = 60.0F;
  wave.archetype = QStringLiteral("cavalry_flank");
  Game::Mission::WaveComposition authored;
  authored.type = QStringLiteral("spearman");
  authored.count = 2;
  wave.composition.push_back(authored);
  ai.waves.push_back(wave);
  mission.ai_setups.push_back(ai);

  Game::Systems::LevelSnapshot level;
  const auto waves = Game::Mission::build_pending_mission_waves(
      {.mission = mission,
       .mission_difficulty = QStringLiteral("normal"),
       .level = level,
       .nations = Game::Session::SessionContext::active().nations()});

  ASSERT_EQ(waves.size(), 1U);
  EXPECT_GT(waves[0].composition.size(), 1U);
  EXPECT_GT(Game::Mission::wave_unit_total(waves[0]), 2);
}
