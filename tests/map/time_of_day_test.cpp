#include <QVector3D>

#include <gtest/gtest.h>

#include "map/map_definition.h"

using namespace Game::Map;

namespace {

TEST(TimeOfDayTest, LightingForDayReturnsHighSun) {
  const auto settings = lighting_for_time_of_day(TimeOfDay::Day);
  EXPECT_GT(settings.light_direction.y(), 0.7F);
  EXPECT_FLOAT_EQ(settings.ambient_strength, 0.30F);
  EXPECT_NEAR(settings.light_direction.length(), 1.0F, 1e-5F);
}

TEST(TimeOfDayTest, LightingForMorningReturnsLowSun) {
  const auto settings = lighting_for_time_of_day(TimeOfDay::Morning);
  EXPECT_LT(settings.light_direction.y(), 0.5F);
  EXPECT_FLOAT_EQ(settings.ambient_strength, 0.22F);
  EXPECT_NEAR(settings.light_direction.length(), 1.0F, 1e-5F);
}

TEST(TimeOfDayTest, LightingForAfternoonIsLowerThanDay) {
  const auto day = lighting_for_time_of_day(TimeOfDay::Day);
  const auto afternoon = lighting_for_time_of_day(TimeOfDay::Afternoon);
  EXPECT_LT(afternoon.light_direction.y(), day.light_direction.y());
  EXPECT_FLOAT_EQ(afternoon.ambient_strength, 0.27F);
  EXPECT_NEAR(afternoon.light_direction.length(), 1.0F, 1e-5F);
}

TEST(TimeOfDayTest, LightingForNightHasLowAmbient) {
  const auto settings = lighting_for_time_of_day(TimeOfDay::Night);
  EXPECT_LT(settings.ambient_strength, 0.20F);
  EXPECT_FLOAT_EQ(settings.ambient_strength, 0.12F);
  EXPECT_NEAR(settings.light_direction.length(), 1.0F, 1e-5F);
}

TEST(TimeOfDayTest, AllTimesProduceNormalizedLightDirections) {
  for (auto tod :
       {TimeOfDay::Morning, TimeOfDay::Day, TimeOfDay::Afternoon, TimeOfDay::Night}) {
    const auto s = lighting_for_time_of_day(tod);
    EXPECT_NEAR(s.light_direction.length(), 1.0F, 1e-5F);
  }
}

TEST(TimeOfDayTest, AmbientStrengthOrderingAcrossTimesOfDay) {
  const float morning = lighting_for_time_of_day(TimeOfDay::Morning).ambient_strength;
  const float day = lighting_for_time_of_day(TimeOfDay::Day).ambient_strength;
  const float afternoon =
      lighting_for_time_of_day(TimeOfDay::Afternoon).ambient_strength;
  const float night = lighting_for_time_of_day(TimeOfDay::Night).ambient_strength;

  EXPECT_LT(morning, day);
  EXPECT_LT(afternoon, day);

  EXPECT_LT(night, morning);
  EXPECT_LT(night, afternoon);
}

TEST(TimeOfDayTest, DefaultMapTimeOfDayIsDay) {
  MapDefinition map;
  EXPECT_EQ(map.time_of_day, TimeOfDay::Day);
}

} // namespace
