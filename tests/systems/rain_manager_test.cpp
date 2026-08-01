#include <gtest/gtest.h>

#include "systems/rain_manager.h"

using namespace Game::Systems;
using namespace Game::Map;

class RainManagerTest : public ::testing::Test {
protected:
  void SetUp() override { rain_manager = std::make_unique<RainManager>(); }

  void TearDown() override { rain_manager.reset(); }

  std::unique_ptr<RainManager> rain_manager;
};

TEST_F(RainManagerTest, DefaultStateIsDisabled) {
  EXPECT_FALSE(rain_manager->is_enabled());
  EXPECT_EQ(rain_manager->get_state(), RainState::Clear);
  EXPECT_FLOAT_EQ(rain_manager->get_intensity(), 0.0F);
}

TEST_F(RainManagerTest, ConfigureEnablesRain) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 0.8F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 12345);

  EXPECT_TRUE(rain_manager->is_enabled());
  EXPECT_FLOAT_EQ(rain_manager->get_cycle_duration(), 100.0F);
}

TEST_F(RainManagerTest, ConfigureWithDisabledRain) {
  RainSettings settings;
  settings.enabled = false;

  rain_manager->configure(settings, 12345);

  EXPECT_FALSE(rain_manager->is_enabled());
}

TEST_F(RainManagerTest, UpdateDoesNothingWhenDisabled) {
  RainSettings settings;
  settings.enabled = false;

  rain_manager->configure(settings, 12345);
  rain_manager->update(10.0F);

  EXPECT_EQ(rain_manager->get_state(), RainState::Clear);
  EXPECT_FLOAT_EQ(rain_manager->get_intensity(), 0.0F);
}

TEST_F(RainManagerTest, RainCycleStartsWithFadingIn) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(0.1F);

  EXPECT_EQ(rain_manager->get_state(), RainState::FadingIn);
  EXPECT_TRUE(rain_manager->is_raining());
}

TEST_F(RainManagerTest, RainTransitionsToActive) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(6.0F);

  EXPECT_EQ(rain_manager->get_state(), RainState::Active);
  EXPECT_FLOAT_EQ(rain_manager->get_intensity(), 1.0F);
}

TEST_F(RainManagerTest, RainTransitionsToFadingOut) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(26.0F);

  EXPECT_EQ(rain_manager->get_state(), RainState::FadingOut);
  EXPECT_TRUE(rain_manager->is_raining());
}

TEST_F(RainManagerTest, RainTransitionsToClear) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(35.0F);

  EXPECT_EQ(rain_manager->get_state(), RainState::Clear);
  EXPECT_FALSE(rain_manager->is_raining());
  EXPECT_FLOAT_EQ(rain_manager->get_intensity(), 0.0F);
}

TEST_F(RainManagerTest, RainCycleRepeats) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(101.0F);

  EXPECT_EQ(rain_manager->get_state(), RainState::FadingIn);
  EXPECT_TRUE(rain_manager->is_raining());
}

TEST_F(RainManagerTest, IntensityGraduallyIncreasesInFadeIn) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(2.5F);

  EXPECT_EQ(rain_manager->get_state(), RainState::FadingIn);
  EXPECT_GT(rain_manager->get_intensity(), 0.0F);
  EXPECT_LT(rain_manager->get_intensity(), 1.0F);
}

TEST_F(RainManagerTest, IntensityGraduallyDecreasesInFadeOut) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(27.0F);

  EXPECT_EQ(rain_manager->get_state(), RainState::FadingOut);
  rain_manager->update(1.0F);
  EXPECT_GT(rain_manager->get_intensity(), 0.0F);
  EXPECT_LT(rain_manager->get_intensity(), 1.0F);
}

TEST_F(RainManagerTest, StateChangeCallbackIsCalled) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  int callback_count = 0;
  RainState last_state = RainState::Clear;

  rain_manager->set_state_change_callback([&](RainState new_state) {
    callback_count++;
    last_state = new_state;
  });

  rain_manager->configure(settings, 0);
  rain_manager->update(0.1F);

  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(last_state, RainState::FadingIn);
}

TEST_F(RainManagerTest, ResetClearsState) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(10.0F);
  rain_manager->reset();

  EXPECT_EQ(rain_manager->get_state(), RainState::Clear);
  EXPECT_FLOAT_EQ(rain_manager->get_intensity(), 0.0F);
}

TEST_F(RainManagerTest, DeterministicTimingWithSeed) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  RainManager manager1;
  RainManager manager2;

  manager1.configure(settings, 12345);
  manager2.configure(settings, 12345);

  EXPECT_FLOAT_EQ(manager1.get_cycle_time(), manager2.get_cycle_time());
}

TEST_F(RainManagerTest, WeatherTypeDefaultsToRain) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 0.8F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 12345);

  EXPECT_EQ(rain_manager->get_weather_type(), WeatherType::Rain);
}

TEST_F(RainManagerTest, WeatherTypeCanBeSetToSnow) {
  RainSettings settings;
  settings.enabled = true;
  settings.type = WeatherType::Snow;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 0.8F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 12345);

  EXPECT_EQ(rain_manager->get_weather_type(), WeatherType::Snow);
}

TEST_F(RainManagerTest, WindStrengthDefaultsToZero) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 0.8F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 12345);

  EXPECT_FLOAT_EQ(rain_manager->get_wind_strength(), 0.0F);
}

TEST_F(RainManagerTest, WindStrengthCanBeConfigured) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 0.8F;
  settings.fade_duration = 5.0F;
  settings.wind_strength = 0.5F;

  rain_manager->configure(settings, 12345);

  EXPECT_FLOAT_EQ(rain_manager->get_wind_strength(), 0.5F);
}

TEST_F(RainManagerTest, WindDirectionIsCarriedFromTheMapDefinition) {
  RainSettings settings;
  settings.enabled = true;
  settings.wind_direction_deg = 270.0F;

  rain_manager->configure(settings, 12345);

  EXPECT_FLOAT_EQ(rain_manager->get_wind_direction_deg(), 270.0F);

  const QVector3D wind = weather_wind_vector(270.0F);
  EXPECT_NEAR(wind.x(), -1.0F, 1e-5F);
  EXPECT_NEAR(wind.y(), 0.0F, 1e-5F);
  EXPECT_NEAR(wind.z(), 0.0F, 1e-5F);
}

TEST_F(RainManagerTest, SnapshotAndRestoreResumeAnInterruptedCycle) {
  RainSettings settings;
  settings.enabled = true;
  settings.cycle_duration = 100.0F;
  settings.active_duration = 30.0F;
  settings.intensity = 1.0F;
  settings.fade_duration = 5.0F;

  rain_manager->configure(settings, 0);
  rain_manager->update(2.5F);
  ASSERT_EQ(rain_manager->get_state(), RainState::FadingIn);
  const RainRuntimeState saved = rain_manager->snapshot();
  const float saved_intensity = rain_manager->get_intensity();

  RainManager reloaded;
  reloaded.configure(settings, 0);
  EXPECT_EQ(reloaded.get_state(), RainState::Clear);

  reloaded.restore(saved);

  EXPECT_EQ(reloaded.get_state(), RainState::FadingIn);
  EXPECT_FLOAT_EQ(reloaded.get_cycle_time(), rain_manager->get_cycle_time());
  EXPECT_FLOAT_EQ(reloaded.get_intensity(), saved_intensity);
}

TEST_F(RainManagerTest, RestoreOnAClearMapCannotResurrectWeather) {
  RainSettings settings;
  settings.enabled = false;

  rain_manager->configure(settings, 0);
  rain_manager->restore({.state = RainState::Active,
                         .cycle_time = 10.0F,
                         .state_time = 4.0F,
                         .intensity = 0.9F});

  EXPECT_EQ(rain_manager->get_state(), RainState::Clear);
  EXPECT_FLOAT_EQ(rain_manager->get_intensity(), 0.0F);
}

TEST_F(RainManagerTest, RainStateNamesRoundTrip) {
  for (const RainState state : {RainState::Clear,
                                RainState::FadingIn,
                                RainState::Active,
                                RainState::FadingOut}) {
    EXPECT_EQ(parse_rain_state(QString::fromLatin1(rain_state_name(state))), state);
  }
  EXPECT_EQ(parse_rain_state(QStringLiteral("nonsense")), RainState::Clear);
}
