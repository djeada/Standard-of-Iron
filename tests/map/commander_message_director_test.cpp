#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <gtest/gtest.h>

#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/mission/commander_message_director.h"
#include "game/mission/commander_voice_bank.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"

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

TEST_F(CommanderMessageDirectorTest, TheOutcomeLineSpeaksAndFlushesTheChatterBehindIt) {
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
  m_director.update(Game::Mission::k_commander_chatter_min_gap_seconds + 1.0F);
  EXPECT_FALSE(m_director.has_active())
      << "a capture quip has no place after the loser's last word";
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

TEST_F(CommanderMessageDirectorTest, LongLinesStayUpLongEnoughToTypeAndRead) {
  Game::Mission::MissionDefinition mission;
  auto message = make_message(QStringLiteral("open"),
                              Game::Mission::CommanderMessageTrigger::MissionStart);
  message.text = QString(240, QLatin1Char('a'));
  message.duration = 9.0F;
  mission.commander_messages.push_back(message);
  configure(std::move(mission));

  m_director.notify_mission_start();
  EXPECT_TRUE(m_director.update(0.0F));
  ASSERT_TRUE(m_director.has_active());

  const float typing =
      240.0F * Game::Mission::k_commander_message_type_seconds_per_char;
  const float reading =
      240.0F * Game::Mission::k_commander_message_read_seconds_per_char;
  EXPECT_GE(m_director.active().duration, typing + reading);
  EXPECT_LE(m_director.active().duration,
            Game::Mission::k_commander_message_max_seconds);
}

TEST_F(CommanderMessageDirectorTest, ShortLinesKeepTheirAuthoredDuration) {
  Game::Mission::MissionDefinition mission;
  auto message = make_message(QStringLiteral("open"),
                              Game::Mission::CommanderMessageTrigger::MissionStart);
  message.text = QStringLiteral("Hold.");
  message.duration = 9.0F;
  mission.commander_messages.push_back(message);
  configure(std::move(mission));

  m_director.notify_mission_start();
  EXPECT_TRUE(m_director.update(0.0F));
  ASSERT_TRUE(m_director.has_active());
  EXPECT_FLOAT_EQ(m_director.active().duration, 9.0F);
}

constexpr int k_ally_owner = 4;

auto make_line(const QString& id,
               Game::Mission::CommanderRelationship relationship,
               Game::Mission::CommanderMessageTrigger trigger,
               QStringList variants) -> Game::Mission::CommanderVoiceLine {
  Game::Mission::CommanderVoiceLine line;
  line.id = id;
  line.relationship = relationship;
  line.trigger = trigger;
  line.variants = std::move(variants);
  line.duration = 4.0F;
  line.priority = 40;
  return line;
}

class CommanderVoiceDirectorTest : public CommanderMessageDirectorTest {
protected:
  void SetUp() override {
    Game::Mission::CommanderVoiceBank scipio;
    scipio.commander_id = QStringLiteral("roman_veteran_consul");
    scipio.chatter_per_match = 2;
    m_scipio = std::move(scipio);

    Game::Mission::CommanderVoiceBank hannibal;
    hannibal.commander_id = QStringLiteral("carthage_sword_commander");
    m_hannibal = std::move(hannibal);

    m_director.set_relationship_lookup([](int a, int b) {
      const auto team = [](int owner) {
        return owner == k_local_owner || owner == k_ally_owner ? 1 : 2;
      };
      return team(a) == team(b);
    });
  }

  void configure_script(std::vector<Game::Mission::CommanderMessage> mission_lines = {},
                        Game::Mission::CommanderVoicesPolicy policy = {}) {
    m_library = Game::Mission::CommanderVoiceLibrary{};
    m_library.add(m_scipio);
    m_library.add(m_hannibal);
    Game::Mission::CommanderMessageScript script;
    script.mission_lines = std::move(mission_lines);
    script.speakers = {{.owner_id = k_hill_fort_owner,
                        .troop_type = QStringLiteral("roman_veteran_consul"),
                        .relationship = Game::Mission::CommanderRelationship::Enemy},
                       {.owner_id = k_ally_owner,
                        .troop_type = QStringLiteral("carthage_sword_commander"),
                        .relationship = Game::Mission::CommanderRelationship::Ally}};
    script.voices = &m_library;
    script.policy = std::move(policy);
    m_director.configure(script, k_local_owner, identity_to_world());
  }

  void capture(int from, int to) {
    m_director.notify_fact(
        {.trigger = Game::Mission::CommanderMessageTrigger::StructureCaptured,
         .subject_owner_id = from,
         .actor_owner_id = to});
  }

  Game::Mission::CommanderVoiceBank m_scipio;
  Game::Mission::CommanderVoiceBank m_hannibal;
  Game::Mission::CommanderVoiceLibrary m_library;
};

TEST_F(CommanderVoiceDirectorTest, RoleFiltersResolveAgainstTheSpeakersOwner) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("You have that camp.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.condition.actor_role = Game::Mission::CommanderMessageRole::Player;
  lost.once = false;
  lost.condition.cooldown = 1.0F;
  m_scipio.lines.push_back(lost);
  configure_script();

  capture(k_river_town_owner, k_local_owner);
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active()) << "somebody else's camp is not Scipio's";

  capture(k_hill_fort_owner, k_ally_owner);
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active()) << "the ally took it, not the player";

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("2:scipio.enemy.lost_camp.1"));
  EXPECT_EQ(m_director.active().relationship, QStringLiteral("enemy"));
  EXPECT_EQ(m_director.active().speaker_owner_id, k_hill_fort_owner);
  EXPECT_STREQ(m_director.active().text_context,
               Game::Util::k_commander_voices_context);
}

TEST_F(CommanderVoiceDirectorTest, AllyLinesCarryTheRelationshipAndOnlyBindToAllies) {
  auto took = make_line(QStringLiteral("hannibal.ally.took_camp"),
                        Game::Mission::CommanderRelationship::Ally,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("That camp is ours.")});
  took.condition.actor_role = Game::Mission::CommanderMessageRole::Self;
  m_hannibal.lines.push_back(took);

  auto enemy_only = make_line(QStringLiteral("hannibal.enemy.took_camp"),
                              Game::Mission::CommanderRelationship::Enemy,
                              Game::Mission::CommanderMessageTrigger::StructureCaptured,
                              {QStringLiteral("Mine now.")});
  enemy_only.condition.actor_role = Game::Mission::CommanderMessageRole::Self;
  enemy_only.priority = 99;
  m_hannibal.lines.push_back(enemy_only);
  configure_script();

  capture(k_hill_fort_owner, k_ally_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().id, QStringLiteral("4:hannibal.ally.took_camp.1"));
  EXPECT_EQ(m_director.active().relationship, QStringLiteral("ally"));
  EXPECT_EQ(m_director.active().speaker_name, QStringLiteral("Hannibal Barca"));
}

TEST_F(CommanderVoiceDirectorTest, VariantsPlayInAuthoredOrderThenCycle) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("First."), QStringLiteral("Second.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.once = false;
  lost.condition.cooldown = 1.0F;
  lost.priority = 90;
  m_scipio.lines.push_back(lost);
  configure_script();

  QStringList shown;
  for (int round = 0; round < 3; ++round) {
    capture(k_hill_fort_owner, k_local_owner);
    m_director.update(2.0F);
    ASSERT_TRUE(m_director.has_active()) << "round " << round;
    shown.push_back(m_director.active().text);
    m_director.dismiss_active();
  }
  EXPECT_EQ(shown,
            (QStringList{QStringLiteral("First."),
                         QStringLiteral("Second."),
                         QStringLiteral("First.")}));
}

TEST_F(CommanderVoiceDirectorTest, AMissionLineSilencesTheGenericOneForTheSameBeat) {
  auto generic = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                           Game::Mission::CommanderRelationship::Enemy,
                           Game::Mission::CommanderMessageTrigger::StructureCaptured,
                           {QStringLiteral("Generic.")});
  generic.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  generic.priority = 90;
  m_scipio.lines.push_back(generic);

  auto authored =
      make_message(QStringLiteral("river_town"),
                   Game::Mission::CommanderMessageTrigger::StructureCaptured);
  authored.condition.owner_id = k_hill_fort_owner;
  authored.text = QStringLiteral("Authored.");
  authored.priority = 10;
  configure_script({authored});

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().text, QStringLiteral("Authored."));
  EXPECT_STREQ(m_director.active().text_context, Game::Util::k_missions_context);
  m_director.dismiss_active();
  m_director.update(20.0F);
  EXPECT_FALSE(m_director.has_active()) << "the generic line must not queue behind it";
}

TEST_F(CommanderVoiceDirectorTest, ChatterRespectsTheMinimumGapButOutcomesDoNot) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("One."), QStringLiteral("Two.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.once = false;
  lost.condition.cooldown = 1.0F;
  m_scipio.lines.push_back(lost);
  m_scipio.lines.push_back(
      make_line(QStringLiteral("scipio.enemy.victory"),
                Game::Mission::CommanderRelationship::Enemy,
                Game::Mission::CommanderMessageTrigger::MissionVictory,
                {QStringLiteral("Take the field.")}));
  configure_script();

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  m_director.dismiss_active();

  m_director.update(2.0F);
  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active()) << "a second chatter line inside the gap";

  m_director.update(Game::Mission::k_commander_chatter_min_gap_seconds);
  EXPECT_TRUE(m_director.has_active()) << "the gap has passed";
  m_director.dismiss_active();

  m_director.notify_victory();
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_TRUE(m_director.active().holds_outcome);
}

TEST_F(CommanderVoiceDirectorTest, ACooledDownLineWaits) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("One."), QStringLiteral("Two.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.once = false;
  lost.condition.cooldown = 60.0F;
  m_scipio.lines.push_back(lost);
  configure_script();

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  m_director.dismiss_active();
  m_director.update(30.0F);

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active()) << "the second variant is on the same cooldown";

  m_director.update(31.0F);
  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().text, QStringLiteral("Two."));
}

TEST_F(CommanderVoiceDirectorTest, StaleChatterExpiresWhileALongerLineHolds) {
  m_scipio.lines.push_back(
      make_line(QStringLiteral("scipio.enemy.match_start"),
                Game::Mission::CommanderRelationship::Enemy,
                Game::Mission::CommanderMessageTrigger::MissionStart,
                {QStringLiteral("Long opening.")}));
  m_scipio.lines.back().duration = 30.0F;
  m_scipio.lines.back().priority = 100;
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("Stale.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  m_scipio.lines.push_back(lost);
  configure_script();

  m_director.notify_mission_start();
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(Game::Mission::k_commander_chatter_expiry_seconds + 1.0F);
  m_director.dismiss_active();
  m_director.update(Game::Mission::k_commander_chatter_min_gap_seconds + 1.0F);
  EXPECT_FALSE(m_director.has_active()) << "the capture line should have been dropped";
}

TEST_F(CommanderVoiceDirectorTest, AnOutcomeLinePreemptsChatter) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("Chatter.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.duration = 30.0F;
  m_scipio.lines.push_back(lost);
  m_scipio.lines.push_back(
      make_line(QStringLiteral("scipio.enemy.defeat"),
                Game::Mission::CommanderRelationship::Enemy,
                Game::Mission::CommanderMessageTrigger::MissionDefeat,
                {QStringLiteral("Sit down in the mud.")}));
  configure_script();

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().text, QStringLiteral("Chatter."));

  m_director.notify_defeat();
  EXPECT_TRUE(m_director.update(0.0F));
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().text, QStringLiteral("Sit down in the mud."));

  capture(k_hill_fort_owner, k_local_owner);
  m_director.dismiss_active();
  m_director.update(30.0F);
  EXPECT_FALSE(m_director.has_active()) << "no chatter after the outcome";
}

TEST_F(CommanderVoiceDirectorTest, ChatterBudgetCapsASpeaker) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("Again.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.once = false;
  lost.condition.cooldown = 1.0F;
  m_scipio.lines.push_back(lost);
  configure_script();

  int shown = 0;
  for (int round = 0; round < 5; ++round) {
    capture(k_hill_fort_owner, k_local_owner);
    m_director.update(Game::Mission::k_commander_chatter_min_gap_seconds + 1.0F);
    if (m_director.has_active()) {
      ++shown;
      m_director.dismiss_active();
    }
  }
  EXPECT_EQ(shown, 2) << "chatter_per_match was 2";
}

TEST_F(CommanderVoiceDirectorTest, OneSpeakerAnswersAFactAndOpeningsAreForEveryone) {
  m_scipio.lines.push_back(
      make_line(QStringLiteral("scipio.enemy.match_start"),
                Game::Mission::CommanderRelationship::Enemy,
                Game::Mission::CommanderMessageTrigger::MissionStart,
                {QStringLiteral("Scipio opens.")}));
  m_hannibal.lines.push_back(
      make_line(QStringLiteral("hannibal.ally.match_start"),
                Game::Mission::CommanderRelationship::Ally,
                Game::Mission::CommanderMessageTrigger::MissionStart,
                {QStringLiteral("Hannibal opens.")}));
  auto scipio_took =
      make_line(QStringLiteral("scipio.enemy.took_camp"),
                Game::Mission::CommanderRelationship::Enemy,
                Game::Mission::CommanderMessageTrigger::StructureCaptured,
                {QStringLiteral("Mine.")});
  scipio_took.condition.actor_role = Game::Mission::CommanderMessageRole::Self;
  m_scipio.lines.push_back(scipio_took);
  auto hannibal_saw =
      make_line(QStringLiteral("hannibal.ally.player_lost_camp"),
                Game::Mission::CommanderRelationship::Ally,
                Game::Mission::CommanderMessageTrigger::StructureCaptured,
                {QStringLiteral("They took yours.")});
  hannibal_saw.condition.subject_role = Game::Mission::CommanderMessageRole::Player;
  hannibal_saw.priority = 60;
  m_hannibal.lines.push_back(hannibal_saw);
  configure_script();

  m_director.notify_mission_start();
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  m_director.dismiss_active();
  EXPECT_TRUE(m_director.has_active()) << "both openings play, one after the other";
  m_director.dismiss_active();
  m_director.update(Game::Mission::k_commander_chatter_min_gap_seconds + 1.0F);

  capture(k_local_owner, k_hill_fort_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().text, QStringLiteral("They took yours."))
      << "the louder speaker answers";
  m_director.dismiss_active();
  m_director.update(Game::Mission::k_commander_chatter_min_gap_seconds + 1.0F);
  EXPECT_FALSE(m_director.has_active()) << "the quieter answer was never queued";
}

TEST_F(CommanderVoiceDirectorTest, AMutedBankLineIsNeverInstantiated) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("Muted.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  m_scipio.lines.push_back(lost);
  Game::Mission::CommanderVoicesPolicy policy;
  policy.muted_lines.push_back(QStringLiteral("scipio.enemy.lost_camp"));
  configure_script({}, policy);

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());

  policy.muted_lines.clear();
  policy.generic = false;
  configure_script({}, policy);
  EXPECT_FALSE(m_director.has_messages()) << "generic:false leaves no bank rules";
}

TEST_F(CommanderVoiceDirectorTest, RestoreCarriesSpentVariantsCooldownsAndBudget) {
  auto lost = make_line(QStringLiteral("scipio.enemy.lost_camp"),
                        Game::Mission::CommanderRelationship::Enemy,
                        Game::Mission::CommanderMessageTrigger::StructureCaptured,
                        {QStringLiteral("One."), QStringLiteral("Two.")});
  lost.condition.subject_role = Game::Mission::CommanderMessageRole::Self;
  lost.once = false;
  lost.condition.cooldown = 60.0F;
  m_scipio.lines.push_back(lost);
  configure_script();

  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  m_director.dismiss_active();
  const QJsonObject state = m_director.serialize();
  EXPECT_EQ(state["chatter_spent"].toObject()[QStringLiteral("2")].toInt(), 1);

  configure_script();
  m_director.restore(state);
  m_director.update(10.0F);
  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active()) << "the cooldown survived the save";

  m_director.update(60.0F);
  capture(k_hill_fort_owner, k_local_owner);
  m_director.update(0.0F);
  ASSERT_TRUE(m_director.has_active());
  EXPECT_EQ(m_director.active().text, QStringLiteral("Two."))
      << "the first variant stayed spent";
}

TEST_F(CommanderMessageDirectorTest, RestoringAnOldSaveShapeStillWorks) {
  Game::Mission::MissionDefinition mission;
  mission.commander_messages.push_back(make_message(
      QStringLiteral("open"), Game::Mission::CommanderMessageTrigger::MissionStart));
  configure(std::move(mission));

  QJsonObject legacy;
  legacy["fired"] = QJsonArray{QStringLiteral("open")};
  m_director.restore(legacy);

  m_director.notify_mission_start();
  m_director.update(0.0F);
  EXPECT_FALSE(m_director.has_active());
}

} // namespace
