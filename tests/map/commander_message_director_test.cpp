#include <QJsonObject>

#include <gtest/gtest.h>

#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/mission/commander_message_director.h"
#include "game/units/spawn_type.h"

namespace {

constexpr int k_local_owner = 1;
constexpr int k_hill_fort_owner = 2;
constexpr int k_river_town_owner = 3;

auto identity_to_world() -> Game::Mission::MissionPositionToWorld {
  return [](const Game::Mission::Position& position) {
    return QVector3D(position.x, 0.0F, position.z);
  };
}

auto make_message(const QString& id, Game::Mission::CommanderMessageTrigger trigger)
    -> Game::Mission::CommanderMessage {
  Game::Mission::CommanderMessage message;
  message.id = id;
  message.speaker = QStringLiteral("roman_veteran_consul");
  message.text = QStringLiteral("Cross where you like.");
  message.trigger = trigger;
  message.duration = 4.0F;
  return message;
}

class CommanderMessageDirectorTest : public ::testing::Test {
protected:
  void TearDown() override {
    m_director.clear();
    Engine::Core::EventManager::instance().clear_all_subscriptions();
  }

  void configure(Game::Mission::MissionDefinition mission) {
    m_mission = std::move(mission);
    m_director.configure(m_mission, k_local_owner, identity_to_world());
  }

  Game::Mission::MissionDefinition m_mission;
  Game::Mission::CommanderMessageDirector m_director;
};

TEST_F(CommanderMessageDirectorTest, MissionStartLineResolvesItsSpeakerFromTheCatalog) {
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(make_message(
      QStringLiteral("open"), Game::Mission::CommanderMessageTrigger::MissionStart));
  configure(std::move(mission));

  m_director.notify_mission_start();
  EXPECT_TRUE(m_director.update(0.0F));
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("open"));
  EXPECT_EQ(m_director.active().speaker_name,
            QStringLiteral("Publius Cornelius Scipio"));
  EXPECT_EQ(m_director.active().nation, QStringLiteral("roman_republic"));
}

TEST_F(CommanderMessageDirectorTest, ADelayedLineWaitsBeforeItIsShown) {
  auto message = make_message(QStringLiteral("open"),
                              Game::Mission::CommanderMessageTrigger::MissionStart);
  message.delay = 2.5F;
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(message);
  configure(std::move(mission));

  m_director.notify_mission_start();
  m_director.update(1.0F);
  EXPECT_FALSE(m_director.has_active());

  m_director.update(2.0F);
  EXPECT_TRUE(m_director.has_active());
}

TEST_F(CommanderMessageDirectorTest, ALineClearsItselfWhenItsDwellRunsOut) {
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(make_message(
      QStringLiteral("open"), Game::Mission::CommanderMessageTrigger::MissionStart));
  configure(std::move(mission));

  m_director.notify_mission_start();
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());

  m_director.update(3.0F);
  EXPECT_TRUE(m_director.has_active());
  EXPECT_TRUE(m_director.update(1.5F));
  EXPECT_FALSE(m_director.has_active());
}

TEST_F(CommanderMessageDirectorTest, AOnceLineNeverPlaysTwice) {
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(make_message(
      QStringLiteral("open"), Game::Mission::CommanderMessageTrigger::MissionStart));
  configure(std::move(mission));

  m_director.notify_mission_start();
  m_director.update(0.0F);
  m_director.dismiss_active();

  m_director.notify_mission_start();
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());
}

TEST_F(CommanderMessageDirectorTest, CaptureLineFiresOnlyForTheNamedOwners) {
  auto message =
      make_message(QStringLiteral("river_town"),
                   Game::Mission::CommanderMessageTrigger::StructureCaptured);
  message.condition.owner_id = k_river_town_owner;
  message.condition.by_owner_is_local = true;
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(message);
  configure(std::move(mission));

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(10, k_hill_fort_owner, k_local_owner));
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(11, k_river_town_owner, k_hill_fort_owner));
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(12, k_river_town_owner, k_local_owner));
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("river_town"));
}

TEST_F(CommanderMessageDirectorTest, CommanderDeathLineFiltersOnNationAndKiller) {
  auto message =
      make_message(QStringLiteral("consul_down"),
                   Game::Mission::CommanderMessageTrigger::CommanderDefeated);
  message.condition.nation = QStringLiteral("roman_republic");
  message.condition.by_owner_is_local = true;
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(message);
  configure(std::move(mission));

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitDiedEvent(
      20, k_hill_fort_owner, Game::Units::SpawnType::Knight, 1, k_local_owner));
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(21,
                                  k_local_owner,
                                  Game::Units::SpawnType::CarthageSwordCommander,
                                  2,
                                  k_hill_fort_owner));
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(22,
                                  k_hill_fort_owner,
                                  Game::Units::SpawnType::RomanVeteranConsul,
                                  3,
                                  k_local_owner));
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("consul_down"));
}

TEST_F(CommanderMessageDirectorTest, APlacedCaptureLineIgnoresACampElsewhere) {
  auto message =
      make_message(QStringLiteral("hill_fort"),
                   Game::Mission::CommanderMessageTrigger::StructureCaptured);
  message.condition.at = Game::Mission::Position{.x = 376.0F, .z = 44.0F};
  message.condition.radius = 30.0F;
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(message);
  configure(std::move(mission));

  m_director.set_structure_position_lookup(
      [](Engine::Core::EntityID id) -> std::optional<QVector3D> {
        if (id == 30) {
          return QVector3D(566.0F, 0.0F, 545.0F);
        }
        return QVector3D(380.0F, 0.0F, 50.0F);
      });

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(30, k_river_town_owner, k_local_owner));
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(31, k_hill_fort_owner, k_local_owner));
  m_director.update(0.0F);
  EXPECT_TRUE(m_director.has_active());
}

TEST_F(CommanderMessageDirectorTest, TheLouderLineIsShownFirst) {
  auto quiet = make_message(QStringLiteral("capture"),
                            Game::Mission::CommanderMessageTrigger::StructureCaptured);
  quiet.priority = 10;
  auto loud = make_message(QStringLiteral("victory"),
                           Game::Mission::CommanderMessageTrigger::MissionVictory);
  loud.priority = 120;

  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(quiet);
  mission.commander_messages.push_back(loud);
  configure(std::move(mission));

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(40, k_river_town_owner, k_local_owner));
  m_director.notify_victory();
  m_director.update(0.0F);

  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("victory"));

  m_director.dismiss_active();
  EXPECT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("capture"));
}

TEST_F(CommanderMessageDirectorTest, RestoringASaveDoesNotReplayASpentLine) {
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(make_message(
      QStringLiteral("open"), Game::Mission::CommanderMessageTrigger::MissionStart));
  configure(std::move(mission));

  m_director.notify_mission_start();
  m_director.update(0.0F);
  const QJsonObject state = m_director.serialize();

  m_director.restore(state);
  EXPECT_FALSE(m_director.has_active());

  m_director.notify_mission_start();
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());
}

} // namespace
