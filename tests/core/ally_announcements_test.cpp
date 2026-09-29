#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "app/world/ally_announcements.h"
#include "game/session/session_context.h"
#include "game/systems/alliance_board.h"
#include "game/systems/economy/marketplace_system.h"
#include "game/systems/owner_registry.h"

namespace {

using Game::Mission::CommanderMessageTrigger;
using Game::Systems::AllyAppeal;
using Game::Systems::AllyAppealFollowUp;
using Game::Systems::AllyAppealKind;
using Game::Systems::AllyAppealReply;
using Game::Systems::AllyCallAnswer;
using Game::Systems::AllyCallKind;
using Game::Systems::AllyCallVerdict;
using Game::Systems::AllyTributeAnswer;
using Game::Systems::AllyTributeVerdict;
using Game::Systems::ResourceType;

constexpr int k_local = 1;
constexpr int k_hanno = 3;
constexpr int k_mago = 4;

struct Exchange {
  QString text;
  bool positive = false;
};

class AllyAnnouncementsTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = m_session.owners();
    owners.register_owner_with_id(k_local, Game::Systems::OwnerType::Player, "player");
    owners.register_owner_with_id(k_hanno, Game::Systems::OwnerType::AI, "Hanno");
    owners.register_owner_with_id(k_mago, Game::Systems::OwnerType::AI, "Mago");
    owners.set_owner_team(k_local, 1);
    owners.set_owner_team(k_hanno, 1);
    owners.set_owner_team(k_mago, 1);
    m_presenter = std::make_unique<App::World::AllyAnnouncementPresenter>(
        App::World::AllyAnnouncementSink{
            .exchange =
                [this](const QString& text, bool positive) {
                  m_exchanges.push_back({text, positive});
                },
            .appeal_opened =
                [this](const QVariantMap& card) { m_opened.push_back(card); },
            .appeal_closed = [this](quint32 id) { m_closed.push_back(id); },
            .commander_fact =
                [this](const Game::Mission::CommanderMessageFact& fact) {
                  m_facts.push_back(fact);
                }});
  }

  Game::Session::SessionContext m_session;
  std::unique_ptr<App::World::AllyAnnouncementPresenter> m_presenter;
  std::vector<Exchange> m_exchanges;
  std::vector<QVariantMap> m_opened;
  std::vector<quint32> m_closed;
  std::vector<Game::Mission::CommanderMessageFact> m_facts;
};

TEST_F(AllyAnnouncementsTest, ASentGiftIsNarratedOnceForTheGiver) {
  m_session.marketplace().record_ally_answer({.requester = k_hanno,
                                              .giver = k_local,
                                              .resource = ResourceType::Wood,
                                              .requested = 100,
                                              .granted = 100,
                                              .verdict = AllyTributeVerdict::Sent});
  m_presenter->announce_exchanges(&m_session, k_local);

  ASSERT_EQ(m_exchanges.size(), 1U);
  EXPECT_EQ(m_exchanges.front().text, QStringLiteral("Sent 100 wood to Hanno."));
  EXPECT_TRUE(m_exchanges.front().positive);
  ASSERT_EQ(m_facts.size(), 1U);
  EXPECT_EQ(m_facts.front().trigger, CommanderMessageTrigger::GiftReceived);
  EXPECT_EQ(m_facts.front().subject_owner_id, k_hanno);
  EXPECT_EQ(m_facts.front().actor_owner_id, k_local);
  EXPECT_EQ(m_facts.front().amount, 100);

  m_presenter->announce_exchanges(&m_session, k_local);
  EXPECT_EQ(m_exchanges.size(), 1U) << "answers are drained on read, never repeated";
}

TEST_F(AllyAnnouncementsTest, AGiftSentByAnotherOwnerIsSilent) {
  m_session.marketplace().record_ally_answer({.requester = k_local,
                                              .giver = k_hanno,
                                              .resource = ResourceType::Gold,
                                              .requested = 0,
                                              .granted = 50,
                                              .verdict = AllyTributeVerdict::Sent});
  m_presenter->announce_exchanges(&m_session, k_local);
  EXPECT_TRUE(m_exchanges.empty());
  EXPECT_TRUE(m_facts.empty());
}

TEST_F(AllyAnnouncementsTest, RepliesToTheLocalRequestCarryTheVerdictAsAFact) {
  const auto answer = [this](AllyTributeVerdict verdict, int granted) {
    m_session.marketplace().record_ally_answer({.requester = k_local,
                                                .giver = k_hanno,
                                                .resource = ResourceType::Gold,
                                                .requested = 80,
                                                .granted = granted,
                                                .verdict = verdict});
  };
  answer(AllyTributeVerdict::Granted, 80);
  answer(AllyTributeVerdict::Partial, 30);
  answer(AllyTributeVerdict::RefusedShort, 0);
  answer(AllyTributeVerdict::RefusedStingy, 0);
  m_presenter->announce_exchanges(&m_session, k_local);

  ASSERT_EQ(m_exchanges.size(), 4U);
  EXPECT_EQ(m_exchanges[0].text, QStringLiteral("Hanno sends you 80 gold."));
  EXPECT_EQ(m_exchanges[1].text,
            QStringLiteral("Hanno can spare only 30 of the 80 gold you asked for."));
  EXPECT_EQ(m_exchanges[2].text, QStringLiteral("Hanno has no gold to spare."));
  EXPECT_EQ(m_exchanges[3].text,
            QStringLiteral("Hanno refuses to part with any gold."));
  EXPECT_TRUE(m_exchanges[0].positive);
  EXPECT_TRUE(m_exchanges[1].positive);
  EXPECT_FALSE(m_exchanges[2].positive);
  EXPECT_FALSE(m_exchanges[3].positive);

  ASSERT_EQ(m_facts.size(), 4U);
  EXPECT_EQ(m_facts[0].trigger, CommanderMessageTrigger::RequestGranted);
  EXPECT_EQ(m_facts[0].reason.value_or(QString()), QStringLiteral("full"));
  EXPECT_EQ(m_facts[1].reason.value_or(QString()), QStringLiteral("partial"));
  EXPECT_EQ(m_facts[2].trigger, CommanderMessageTrigger::RequestRefused);
  EXPECT_EQ(m_facts[2].reason.value_or(QString()), QStringLiteral("short"));
  EXPECT_EQ(m_facts[2].amount, 80) << "a refusal reports what was asked for";
  EXPECT_EQ(m_facts[3].reason.value_or(QString()), QStringLiteral("stingy"));
}

TEST_F(AllyAnnouncementsTest, TradesBetweenTwoAiAlliesAreNarratedWithoutFacts) {
  m_session.marketplace().record_ally_answer({.requester = k_hanno,
                                              .giver = k_mago,
                                              .resource = ResourceType::Iron,
                                              .requested = 40,
                                              .granted = 40,
                                              .verdict = AllyTributeVerdict::Granted});
  m_session.marketplace().record_ally_answer(
      {.requester = k_mago,
       .giver = k_hanno,
       .resource = ResourceType::Iron,
       .requested = 40,
       .granted = 0,
       .verdict = AllyTributeVerdict::RefusedShort});
  m_presenter->announce_exchanges(&m_session, k_local);

  ASSERT_EQ(m_exchanges.size(), 2U);
  EXPECT_EQ(m_exchanges[0].text,
            QStringLiteral("Hanno asked Mago for 40 iron and received 40."));
  EXPECT_TRUE(m_exchanges[0].positive);
  EXPECT_EQ(m_exchanges[1].text,
            QStringLiteral("Mago asked Hanno for 40 iron; Hanno refused."));
  EXPECT_FALSE(m_exchanges[1].positive);
  EXPECT_TRUE(m_facts.empty());
}

TEST_F(AllyAnnouncementsTest, TradesInvolvingAnEnemyCommanderAreNotNarrated) {
  m_session.owners().set_owner_team(k_mago, 2);
  m_session.marketplace().record_ally_answer({.requester = k_hanno,
                                              .giver = k_mago,
                                              .resource = ResourceType::Iron,
                                              .requested = 40,
                                              .granted = 40,
                                              .verdict = AllyTributeVerdict::Granted});
  m_presenter->announce_exchanges(&m_session, k_local);
  EXPECT_TRUE(m_exchanges.empty());
}

TEST_F(AllyAnnouncementsTest,
       CallRepliesAreSummarisedPerCallAndTheFirstAcceptorSpeaks) {
  auto& board = m_session.alliance();
  const auto call_id = board.next_call_id();
  board.record_call_answer({.call_id = call_id,
                            .requester = k_local,
                            .ally = k_mago,
                            .kind = AllyCallKind::Defend,
                            .verdict = AllyCallVerdict::RefusedNoArmy});
  board.record_call_answer({.call_id = call_id,
                            .requester = k_local,
                            .ally = k_hanno,
                            .kind = AllyCallKind::Defend,
                            .verdict = AllyCallVerdict::Accepted});
  m_presenter->announce_calls(&m_session, k_local);

  ASSERT_EQ(m_exchanges.size(), 1U);
  EXPECT_EQ(m_exchanges.front().text,
            QStringLiteral("Hanno will send men to hold it."));
  EXPECT_TRUE(m_exchanges.front().positive);
  ASSERT_EQ(m_facts.size(), 1U);
  EXPECT_EQ(m_facts.front().trigger, CommanderMessageTrigger::CallAccepted);
  EXPECT_EQ(m_facts.front().actor_owner_id, k_hanno);
  EXPECT_EQ(m_facts.front().subject_type, QStringLiteral("defend"));
}

TEST_F(AllyAnnouncementsTest, ARefusedCallNamesTheFirstRefusalAsTheReason) {
  auto& board = m_session.alliance();
  const auto call_id = board.next_call_id();
  board.record_call_answer({.call_id = call_id,
                            .requester = k_local,
                            .ally = k_hanno,
                            .kind = AllyCallKind::Attack,
                            .verdict = AllyCallVerdict::RefusedUnderThreat});
  m_presenter->announce_calls(&m_session, k_local);

  ASSERT_EQ(m_exchanges.size(), 1U);
  EXPECT_EQ(m_exchanges.front().text, QStringLiteral("No ally will join the attack."));
  EXPECT_FALSE(m_exchanges.front().positive);
  ASSERT_EQ(m_facts.size(), 1U);
  EXPECT_EQ(m_facts.front().trigger, CommanderMessageTrigger::CallRefused);
  EXPECT_EQ(m_facts.front().reason.value_or(QString()), QStringLiteral("under_threat"));
}

TEST_F(AllyAnnouncementsTest, AResourceAppealOpensACardAndFeedsTheCommanderVoice) {
  auto& board = m_session.alliance();
  board.record_appeal({.appeal_id = board.next_appeal_id(),
                       .from_ally = k_hanno,
                       .to_owner = k_local,
                       .kind = AllyAppealKind::Resources,
                       .resource = ResourceType::Food,
                       .amount = 60});
  board.record_appeal({.appeal_id = board.next_appeal_id(),
                       .from_ally = k_mago,
                       .to_owner = k_hanno,
                       .kind = AllyAppealKind::Defend});
  m_presenter->announce_appeals(&m_session, k_local);

  ASSERT_EQ(m_opened.size(), 1U) << "appeals addressed to other owners are ignored";
  const QVariantMap& card = m_opened.front();
  EXPECT_EQ(card.value("from").toInt(), k_hanno);
  EXPECT_EQ(card.value("name").toString(), QStringLiteral("Hanno"));
  EXPECT_EQ(card.value("kind").toString(), QStringLiteral("resources"));
  EXPECT_EQ(card.value("resource").toString(), QStringLiteral("food"));
  EXPECT_EQ(card.value("amount").toInt(), 60);
  EXPECT_EQ(card.value("text").toString(),
            QStringLiteral("Hanno asks you for 60 food."));
  EXPECT_EQ(card.value("accept").toString(), QStringLiteral("Give 60"));
  EXPECT_EQ(card.value("decline").toString(), QStringLiteral("Refuse"));
  ASSERT_EQ(m_facts.size(), 1U);
  EXPECT_EQ(m_facts.front().trigger, CommanderMessageTrigger::AllyNeedsResources);
}

TEST_F(AllyAnnouncementsTest, AppealRepliesCloseTheCardUnlessTheyAreLateAidReports) {
  auto& board = m_session.alliance();
  const auto reply = [&board](std::uint32_t id, AllyAppealFollowUp follow_up) {
    board.record_appeal_reply({.appeal_id = id,
                               .from_ally = k_hanno,
                               .to_owner = k_local,
                               .kind = AllyAppealKind::Defend,
                               .follow_up = follow_up});
  };
  reply(7, AllyAppealFollowUp::HoldAlone);
  reply(8, AllyAppealFollowUp::AidArrived);
  m_presenter->announce_appeals(&m_session, k_local);

  ASSERT_EQ(m_closed.size(), 1U);
  EXPECT_EQ(m_closed.front(), 7U);
  ASSERT_EQ(m_exchanges.size(), 2U);
  EXPECT_EQ(m_exchanges[0].text, QStringLiteral("Hanno will hold the camp alone."));
  EXPECT_FALSE(m_exchanges[0].positive);
  EXPECT_EQ(m_exchanges[1].text,
            QStringLiteral("Hanno: your men have reached the camp. Well met."));
  EXPECT_TRUE(m_exchanges[1].positive);
}

TEST_F(AllyAnnouncementsTest, WithoutASessionNothingIsAnnounced) {
  m_presenter->announce_all(nullptr, k_local);
  EXPECT_TRUE(m_exchanges.empty());
  EXPECT_TRUE(m_opened.empty());
  EXPECT_EQ(App::World::owner_display_name(nullptr, k_hanno),
            QStringLiteral("your ally"));
}

TEST_F(AllyAnnouncementsTest, ResourceWordsFallBackToTheKeyForUnknownResources) {
  EXPECT_EQ(App::World::ally_resource_word(QStringLiteral("stone")),
            QStringLiteral("stone"));
  EXPECT_EQ(App::World::ally_resource_word(QStringLiteral("mana")),
            QStringLiteral("mana"));
  EXPECT_TRUE(App::World::is_friendly_commander(&m_session, k_local, k_hanno));
  EXPECT_FALSE(App::World::is_friendly_commander(&m_session, k_local, k_local));
}

} // namespace
