#include <gtest/gtest.h>

#include "app/world/minimap_events.h"

namespace {

using App::World::TroopsUnderAttackAlarm;

TEST(TroopsUnderAttackAlarmTest, TheFirstHitOnQuietTroopsSoundsTheAlarm) {
  TroopsUnderAttackAlarm alarm;
  EXPECT_TRUE(alarm.note_hit(10.0F, 10.0F, 100.0))
      << "wolves biting builders in a quiet camp must be heard";
}

TEST(TroopsUnderAttackAlarmTest, AnOngoingFightDoesNotRepeatTheAlarm) {
  TroopsUnderAttackAlarm alarm;
  ASSERT_TRUE(alarm.note_hit(10.0F, 10.0F, 100.0));
  for (double t = 101.0; t < 200.0; t += 1.0) {
    EXPECT_FALSE(alarm.note_hit(12.0F, 9.0F, t)) << "at " << t;
  }
}

TEST(TroopsUnderAttackAlarmTest, ASecondFrontSoundsOnceTheCooldownHasPassed) {
  TroopsUnderAttackAlarm alarm;
  ASSERT_TRUE(alarm.note_hit(10.0F, 10.0F, 100.0));
  EXPECT_FALSE(alarm.note_hit(200.0F, 200.0F, 110.0))
      << "a second front inside the cooldown stays on the minimap only";
  EXPECT_TRUE(alarm.note_hit(
      -150.0F, 80.0F, 100.0 + TroopsUnderAttackAlarm::k_cooldown_seconds));
}

TEST(TroopsUnderAttackAlarmTest, AnAreaThatFellQuietAlarmsAgain) {
  TroopsUnderAttackAlarm alarm;
  ASSERT_TRUE(alarm.note_hit(10.0F, 10.0F, 100.0));
  const double later = 100.0 + TroopsUnderAttackAlarm::k_cooldown_seconds +
                       TroopsUnderAttackAlarm::k_quiet_seconds;
  EXPECT_TRUE(alarm.note_hit(10.0F, 10.0F, later));
  alarm.reset();
  EXPECT_TRUE(alarm.note_hit(10.0F, 10.0F, later + 1.0))
      << "a new match starts with a clean alarm";
}

} // namespace
