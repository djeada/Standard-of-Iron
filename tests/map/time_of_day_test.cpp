#include <QVector3D>

#include <gtest/gtest.h>

#include "map/map_definition.h"

using namespace Game::Map;

namespace {

TEST(TimeOfDayTest, LightingForDayReturnsHighSun) {
  const auto settings = lighting_for_time_of_day(TimeOfDay::Day);
  EXPECT_GT(settings.primary_direction.y(), 0.7F);
  EXPECT_FLOAT_EQ(settings.ambient_intensity, 0.30F);
  EXPECT_NEAR(settings.primary_direction.length(), 1.0F, 1e-5F);
}

TEST(TimeOfDayTest, LightingForMorningReturnsLowSun) {
  const auto settings = lighting_for_time_of_day(TimeOfDay::Morning);
  EXPECT_LT(settings.primary_direction.y(), 0.5F);
  EXPECT_FLOAT_EQ(settings.ambient_intensity, 0.22F);
  EXPECT_NEAR(settings.primary_direction.length(), 1.0F, 1e-5F);
}

TEST(TimeOfDayTest, AfternoonIsDefinedBySunHeightNotByAmbient) {
  const auto day = lighting_for_time_of_day(TimeOfDay::Day);
  const auto afternoon = lighting_for_time_of_day(TimeOfDay::Afternoon);

  EXPECT_LT(afternoon.primary_direction.y(), day.primary_direction.y());
  EXPECT_NEAR(afternoon.primary_direction.length(), 1.0F, 1e-5F);

  EXPECT_FLOAT_EQ(afternoon.ambient_intensity, 0.34F);
  EXPECT_GT(afternoon.ambient_intensity, day.ambient_intensity);
  EXPECT_GT(afternoon.primary_intensity, day.primary_intensity);
}

TEST(TimeOfDayTest, NightKeepsAmbientHighEnoughToStayLegible) {
  const auto day = lighting_for_time_of_day(TimeOfDay::Day);
  const auto settings = lighting_for_time_of_day(TimeOfDay::Night);

  EXPECT_FLOAT_EQ(settings.ambient_intensity, 0.30F);
  EXPECT_NEAR(settings.primary_direction.length(), 1.0F, 1e-5F);

  EXPECT_LT(settings.primary_intensity, day.primary_intensity * 0.5F);
  EXPECT_GT(settings.primary_color.z(), settings.primary_color.x());
}

TEST(TimeOfDayTest, AllTimesProduceNormalizedLightDirections) {
  for (auto tod :
       {TimeOfDay::Morning, TimeOfDay::Day, TimeOfDay::Afternoon, TimeOfDay::Night}) {
    const auto s = lighting_for_time_of_day(tod);
    EXPECT_NEAR(s.primary_direction.length(), 1.0F, 1e-5F);
  }
}

TEST(TimeOfDayTest, KeyLightOrderingAcrossTimesOfDay) {
  const auto morning = lighting_for_time_of_day(TimeOfDay::Morning);
  const auto day = lighting_for_time_of_day(TimeOfDay::Day);
  const auto afternoon = lighting_for_time_of_day(TimeOfDay::Afternoon);
  const auto night = lighting_for_time_of_day(TimeOfDay::Night);

  EXPECT_LT(morning.primary_intensity, day.primary_intensity);
  EXPECT_LT(night.primary_intensity, morning.primary_intensity);
  EXPECT_LT(night.primary_intensity, afternoon.primary_intensity);

  EXPECT_LT(morning.ambient_intensity, day.ambient_intensity);
  EXPECT_GE(night.ambient_intensity, morning.ambient_intensity);
}

TEST(TimeOfDayTest, DefaultMapTimeOfDayIsDay) {
  MapDefinition map;
  EXPECT_EQ(map.time_of_day, TimeOfDay::Day);
}

TEST(TimeOfDayTest, PresetsHaveStableArenaLabelsAndRepresentativeClockTimes) {
  EXPECT_STREQ(time_of_day_name(TimeOfDay::Morning), "Morning");
  EXPECT_STREQ(representative_clock_time(TimeOfDay::Morning), "07:00");
  EXPECT_STREQ(time_of_day_name(TimeOfDay::Day), "Day");
  EXPECT_STREQ(representative_clock_time(TimeOfDay::Day), "13:00");
  EXPECT_STREQ(time_of_day_name(TimeOfDay::Afternoon), "Afternoon");
  EXPECT_STREQ(representative_clock_time(TimeOfDay::Afternoon), "17:00");
  EXPECT_STREQ(time_of_day_name(TimeOfDay::Night), "Night");
  EXPECT_STREQ(representative_clock_time(TimeOfDay::Night), "22:00");
}

TEST(TimeOfDayTest, ContinuousLightingInterpolatesAllEnvironmentValues) {
  const auto at_seven = lighting_for_hour(7.0F);
  const auto at_eight = lighting_for_hour(8.0F);
  const auto at_thirteen = lighting_for_hour(13.0F);

  EXPECT_GT(at_eight.primary_intensity, at_seven.primary_intensity);
  EXPECT_LT(at_eight.primary_intensity, at_thirteen.primary_intensity);
  EXPECT_NE(at_eight.primary_color, at_seven.primary_color);
  EXPECT_NE(at_eight.sky_color, at_seven.sky_color);
  EXPECT_GT(at_eight.exposure, at_seven.exposure);
}

TEST(TimeOfDayTest, RainSoftensShadowsAndDrivesWetnessAndFog) {
  const auto clear = lighting_for_hour(13.0F);
  const auto rain =
      lighting_for_hour(13.0F, QStringLiteral("mediterranean_summer"), {.rain = 1.0F});

  EXPECT_LT(rain.primary_intensity, clear.primary_intensity);
  EXPECT_LT(rain.shadow_strength, clear.shadow_strength);
  EXPECT_GT(rain.shadow_softness, clear.shadow_softness);
  EXPECT_GT(rain.fog_density, clear.fog_density);
  EXPECT_FLOAT_EQ(rain.cloud_cover, 0.72F);
  EXPECT_FLOAT_EQ(rain.wetness, 1.0F);
}

TEST(TimeOfDayTest, ClockUsesDeterministicSimulationTimeAndPauses) {
  EnvironmentDefinition definition;
  definition.start_time = 6.0F;
  definition.time_mode = TimeMode::Continuous;
  definition.day_length_seconds = 240.0F;
  EnvironmentClock clock(definition);

  clock.update(10.0F, false);
  EXPECT_FLOAT_EQ(clock.hour(), 7.0F);
  clock.update(100.0F, true);
  EXPECT_FLOAT_EQ(clock.hour(), 7.0F);

  EnvironmentClock second(definition);
  for (int i = 0; i < 10; ++i) {
    second.update(1.0F, false);
  }
  EXPECT_NEAR(second.hour(), clock.hour(), 1e-5F);
}

TEST(TimeOfDayTest, ScriptedTransitionSnapshotRestoresExactly) {
  EnvironmentDefinition definition;
  definition.start_time = 17.0F;
  definition.time_mode = TimeMode::Scripted;
  EnvironmentClock original(definition);
  original.begin_transition(22.0F, 20.0F);
  original.update(7.0F, false);

  EnvironmentClock restored;
  restored.restore(definition, original.snapshot());
  EXPECT_FLOAT_EQ(restored.hour(), original.hour());
  EXPECT_TRUE(restored.transition_active());

  original.update(3.0F, false);
  restored.update(3.0F, false);
  EXPECT_FLOAT_EQ(restored.hour(), original.hour());
}

} // namespace
