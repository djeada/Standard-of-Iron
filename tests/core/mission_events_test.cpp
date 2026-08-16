#include <gtest/gtest.h>

#include "app/mission/mission_setup_coordinator.h"

namespace {

auto make_timer_event(float time, const QString& text) -> Game::Mission::GameEvent {
  Game::Mission::GameEvent event;
  event.trigger.type = QStringLiteral("timer");
  event.trigger.time = time;

  Game::Mission::EventAction action;
  action.type = QStringLiteral("show_message");
  action.text = text;
  event.actions.push_back(action);

  return event;
}

} // namespace

TEST(MissionEventsTest, TimerEventsBecomeScheduledBriefingLines) {
  Game::Mission::MissionDefinition mission;
  mission.id = "crossing_the_rhone";
  mission.events.push_back(make_timer_event(60.0F, QStringLiteral("Second line")));
  mission.events.push_back(make_timer_event(20.0F, QStringLiteral("First line")));

  const auto pending = App::Core::build_pending_mission_events(mission);

  ASSERT_EQ(pending.size(), 2U);
  EXPECT_EQ(pending[0].trigger_time, 20.0F);
  EXPECT_EQ(pending[0].text, QStringLiteral("First line"));
  EXPECT_FALSE(pending[0].fired);
  EXPECT_EQ(pending[1].trigger_time, 60.0F);
  EXPECT_EQ(pending[1].text, QStringLiteral("Second line"));
}

TEST(MissionEventsTest, UnsupportedTriggersAndActionsAreSkipped) {
  Game::Mission::MissionDefinition mission;
  mission.id = "unsupported";

  Game::Mission::GameEvent no_time;
  no_time.trigger.type = QStringLiteral("timer");
  mission.events.push_back(no_time);

  Game::Mission::GameEvent wrong_trigger;
  wrong_trigger.trigger.type = QStringLiteral("unit_enters_radius");
  wrong_trigger.trigger.time = 10.0F;
  mission.events.push_back(wrong_trigger);

  Game::Mission::GameEvent wrong_action;
  wrong_action.trigger.type = QStringLiteral("timer");
  wrong_action.trigger.time = 10.0F;
  Game::Mission::EventAction spawn;
  spawn.type = QStringLiteral("spawn_units");
  wrong_action.actions.push_back(spawn);
  mission.events.push_back(wrong_action);

  mission.events.push_back(make_timer_event(30.0F, QStringLiteral("Kept")));

  const auto pending = App::Core::build_pending_mission_events(mission);

  ASSERT_EQ(pending.size(), 1U);
  EXPECT_EQ(pending[0].text, QStringLiteral("Kept"));
}

TEST(MissionEventsTest, EmptyMessagesAreDropped) {
  Game::Mission::MissionDefinition mission;
  mission.id = "blank";
  mission.events.push_back(make_timer_event(5.0F, QString()));

  EXPECT_TRUE(App::Core::build_pending_mission_events(mission).empty());
}

TEST(MissionEventsTest, ShippedMissionsOnlyUseSupportedEventShapes) {
  Game::Mission::MissionDefinition mission;
  mission.id = "shape_check";
  mission.events.push_back(make_timer_event(1.0F, QStringLiteral("a")));
  mission.events.push_back(make_timer_event(2.0F, QStringLiteral("b")));

  const auto pending = App::Core::build_pending_mission_events(mission);
  EXPECT_EQ(pending.size(), mission.events.size());
}
