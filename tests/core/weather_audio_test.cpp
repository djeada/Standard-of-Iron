#include <gtest/gtest.h>

#include "app/core/weather_audio.h"
#include "game/map/map_definition.h"
#include "game/systems/rain_manager.h"

namespace {

using App::Core::WeatherAudio;

auto settings_for(Game::Map::WeatherType type,
                  bool enabled = true) -> Game::Map::RainSettings {
  Game::Map::RainSettings settings;
  settings.enabled = enabled;
  settings.type = type;
  settings.cycle_duration = 20.0F;
  settings.active_duration = 8.0F;
  settings.fade_duration = 2.0F;
  settings.intensity = 1.0F;
  return settings;
}

TEST(WeatherAudioTest, RainAndSnowUseTheirOwnBeds) {
  EXPECT_EQ(WeatherAudio::bed_for(Game::Map::WeatherType::Rain),
            QStringLiteral("ambient.weather_rain"));
  EXPECT_EQ(WeatherAudio::bed_for(Game::Map::WeatherType::Snow),
            QStringLiteral("ambient.weather_snow"));
}

TEST(WeatherAudioTest, VolumeTracksIntensityAndNeverExceedsItsShareOfTheMix) {
  EXPECT_FLOAT_EQ(WeatherAudio::volume_for(0.0F), 0.0F);
  EXPECT_FLOAT_EQ(WeatherAudio::volume_for(1.0F),
                  WeatherAudio::k_full_intensity_volume);
  EXPECT_GT(WeatherAudio::volume_for(0.75F), WeatherAudio::volume_for(0.25F));

  EXPECT_FLOAT_EQ(WeatherAudio::volume_for(4.0F),
                  WeatherAudio::k_full_intensity_volume);
  EXPECT_FLOAT_EQ(WeatherAudio::volume_for(-1.0F), 0.0F);
}

TEST(WeatherAudioTest, AMapWithoutWeatherNeverStartsALayer) {
  Game::Systems::RainManager rain;
  rain.configure(settings_for(Game::Map::WeatherType::Rain, false), 1234U);

  WeatherAudio audio;
  for (int step = 0; step < 200; ++step) {
    rain.update(0.25F);
    audio.update(&rain);
  }

  EXPECT_TRUE(audio.playing_id().isEmpty());
  EXPECT_FLOAT_EQ(audio.volume(), 0.0F);
}

TEST(WeatherAudioTest, ANullWeatherSystemIsSilentRatherThanACrash) {
  WeatherAudio audio;
  audio.update(nullptr);
  EXPECT_TRUE(audio.playing_id().isEmpty());
  audio.stop();
  EXPECT_TRUE(audio.playing_id().isEmpty());
}

TEST(WeatherAudioTest, SnowIsAudibleOnceTheCycleBringsItUp) {
  Game::Systems::RainManager rain;
  rain.configure(settings_for(Game::Map::WeatherType::Snow), 4321U);

  WeatherAudio audio;
  bool ever_played = false;
  for (int step = 0; step < 400 && !ever_played; ++step) {
    rain.update(0.25F);
    audio.update(&rain);
    ever_played = !audio.playing_id().isEmpty();
  }

  ASSERT_TRUE(ever_played) << "the snow layer never started";
  EXPECT_EQ(audio.playing_id(), QStringLiteral("ambient.weather_snow"));
  EXPECT_GT(audio.volume(), 0.0F);
}

TEST(WeatherAudioTest, TheLayerStopsWhenTheSkyClears) {
  Game::Systems::RainManager rain;
  rain.configure(settings_for(Game::Map::WeatherType::Rain), 99U);

  WeatherAudio audio;
  bool started = false;
  for (int step = 0; step < 400; ++step) {
    rain.update(0.25F);
    audio.update(&rain);
    if (!audio.playing_id().isEmpty()) {
      started = true;
    }
    if (started && rain.get_intensity() < WeatherAudio::k_silence_threshold) {
      break;
    }
  }
  ASSERT_TRUE(started);

  for (int step = 0; step < 400 && !audio.playing_id().isEmpty(); ++step) {
    rain.update(0.25F);
    audio.update(&rain);
  }

  EXPECT_TRUE(audio.playing_id().isEmpty())
      << "still playing " << audio.playing_id().toStdString();
}

} // namespace
