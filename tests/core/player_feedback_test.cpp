#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "app/core/player_feedback.h"

namespace {

using App::Core::PlayerFeedbackBus;
using App::Core::PlayerFeedbackEvent;
using App::Core::PlayerFeedbackType;

auto event_of(PlayerFeedbackType type) -> PlayerFeedbackEvent {
  PlayerFeedbackEvent event;
  event.type = type;
  return event;
}

TEST(PlayerFeedbackBusTest, ListenersSeeEventsBeforePublishReturns) {
  PlayerFeedbackBus bus;
  std::vector<PlayerFeedbackType> seen;
  const auto id = bus.subscribe(
      [&seen](const PlayerFeedbackEvent& event) { seen.push_back(event.type); });
  EXPECT_NE(id, 0U);

  bus.publish(event_of(PlayerFeedbackType::OrderIssued));
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_EQ(seen.front(), PlayerFeedbackType::OrderIssued);
}

TEST(PlayerFeedbackBusTest, EverySubscriberIsNotified) {
  PlayerFeedbackBus bus;
  int audio = 0;
  int vfx = 0;
  static_cast<void>(bus.subscribe([&audio](const PlayerFeedbackEvent&) { ++audio; }));
  static_cast<void>(bus.subscribe([&vfx](const PlayerFeedbackEvent&) { ++vfx; }));

  bus.publish(event_of(PlayerFeedbackType::WeaponContact));
  EXPECT_EQ(audio, 1);
  EXPECT_EQ(vfx, 1);
}

TEST(PlayerFeedbackBusTest, UnsubscribingStopsDelivery) {
  PlayerFeedbackBus bus;
  int calls = 0;
  const auto id = bus.subscribe([&calls](const PlayerFeedbackEvent&) { ++calls; });

  bus.publish(event_of(PlayerFeedbackType::PerfectGuard));
  bus.unsubscribe(id);
  bus.publish(event_of(PlayerFeedbackType::PerfectGuard));

  EXPECT_EQ(calls, 1);
}

TEST(PlayerFeedbackBusTest, DrainReturnsEventsInOrderAndEmptiesTheQueue) {
  PlayerFeedbackBus bus;
  bus.publish(event_of(PlayerFeedbackType::OrderIssued));
  bus.publish(event_of(PlayerFeedbackType::OrderRejected));

  const auto drained = bus.drain();
  ASSERT_EQ(drained.size(), 2U);
  EXPECT_EQ(drained[0].type, PlayerFeedbackType::OrderIssued);
  EXPECT_EQ(drained[1].type, PlayerFeedbackType::OrderRejected);
  EXPECT_LT(drained[0].sequence, drained[1].sequence);
  EXPECT_EQ(bus.pending(), 0U);
  EXPECT_TRUE(bus.drain().empty());
}

TEST(PlayerFeedbackBusTest, ASlowReaderNeverGrowsTheQueueWithoutBound) {
  PlayerFeedbackBus bus;
  for (std::size_t i = 0; i < PlayerFeedbackBus::k_max_pending * 3; ++i) {
    bus.publish(event_of(PlayerFeedbackType::WeaponContact));
  }

  EXPECT_EQ(bus.pending(), PlayerFeedbackBus::k_max_pending);
  EXPECT_GT(bus.dropped(), 0U);

  const auto drained = bus.drain();
  ASSERT_FALSE(drained.empty());
  EXPECT_LT(drained.front().sequence, drained.back().sequence);
}

TEST(PlayerFeedbackBusTest, EveryTypeHasAStableNameForQml) {
  for (const auto type : {PlayerFeedbackType::OrderIssued,
                          PlayerFeedbackType::OrderRejected,
                          PlayerFeedbackType::SelectionChanged,
                          PlayerFeedbackType::AttackCommitted,
                          PlayerFeedbackType::WeaponContact,
                          PlayerFeedbackType::PerfectGuard,
                          PlayerFeedbackType::GuardBroken,
                          PlayerFeedbackType::DodgeSuccess,
                          PlayerFeedbackType::ResourceInsufficient,
                          PlayerFeedbackType::CommanderModeEntered,
                          PlayerFeedbackType::CommanderModeExited}) {
    const char* name = App::Core::player_feedback_type_name(type);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(std::string(name).size(), 3U);
  }
}

TEST(PlayerFeedbackBusTest, TheVariantMapCarriesTheWholeEvent) {
  PlayerFeedbackEvent event;
  event.type = PlayerFeedbackType::GuardBroken;
  event.entity = 42;
  event.strength = 0.75F;
  event.reason = QStringLiteral("stagger");
  event.has_world_position = true;
  event.world_position = QVector3D(1.0F, 2.0F, 3.0F);

  const auto map = App::Core::to_variant_map(event);
  EXPECT_EQ(map.value(QStringLiteral("type")).toString(),
            QStringLiteral("guard_broken"));
  EXPECT_EQ(map.value(QStringLiteral("entity")).toULongLong(), 42ULL);
  EXPECT_FLOAT_EQ(map.value(QStringLiteral("strength")).toFloat(), 0.75F);
  EXPECT_EQ(map.value(QStringLiteral("reason")).toString(), QStringLiteral("stagger"));
  EXPECT_TRUE(map.value(QStringLiteral("hasPosition")).toBool());
  EXPECT_FLOAT_EQ(map.value(QStringLiteral("z")).toFloat(), 3.0F);
}

TEST(PlayerFeedbackBusTest, ClearingDropsPendingEventsButKeepsListeners) {
  PlayerFeedbackBus bus;
  int calls = 0;
  static_cast<void>(bus.subscribe([&calls](const PlayerFeedbackEvent&) { ++calls; }));

  bus.publish(event_of(PlayerFeedbackType::OrderIssued));
  bus.clear();
  EXPECT_EQ(bus.pending(), 0U);

  bus.publish(event_of(PlayerFeedbackType::OrderIssued));
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(bus.pending(), 1U);
}

} // namespace
