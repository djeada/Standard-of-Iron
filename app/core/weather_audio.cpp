#include "weather_audio.h"

#include <algorithm>

#include "audio_resource_loader.h"
#include "game/audio/audio_system.h"
#include "game/systems/rain_manager.h"

namespace App::Core {

auto WeatherAudio::bed_for(Game::Map::WeatherType type) -> QString {
  switch (type) {
  case Game::Map::WeatherType::Snow:
    return QStringLiteral("ambient.weather_snow");
  case Game::Map::WeatherType::Rain:
    break;
  }
  return QStringLiteral("ambient.weather_rain");
}

auto WeatherAudio::volume_for(float intensity) -> float {
  return std::clamp(intensity, 0.0F, 1.0F) * k_full_intensity_volume;
}

void WeatherAudio::update(const Game::Systems::RainManager* rain) {
  if (rain == nullptr || !rain->is_enabled()) {
    stop();
    return;
  }

  const QString wanted = bed_for(rain->get_weather_type());
  const float intensity = rain->get_intensity();

  if (intensity < k_silence_threshold) {
    stop();
    return;
  }

  const float target = volume_for(intensity);

  if (m_playing_id != wanted) {
    stop();
    start(wanted, target);
    return;
  }

  if (!AudioSystem::get_instance().is_sound_playing(wanted.toStdString())) {
    start(wanted, target);
    return;
  }

  if (std::abs(target - m_volume) < k_volume_epsilon) {
    return;
  }
  m_volume = target;
  AudioSystem::get_instance().set_playing_sound_volume(wanted.toStdString(), target);
}

void WeatherAudio::start(const QString& bed_id, float volume) {
  AudioResourceLoader::ensure_audio_resource_loaded(bed_id);
  AudioSystem::get_instance().play_sound(
      bed_id.toStdString(), volume, true, 1, AudioCategory::AMBIENCE);
  m_playing_id = bed_id;
  m_volume = volume;
}

void WeatherAudio::preload(Game::Map::WeatherType type) {
  AudioResourceLoader::ensure_audio_resource_loaded(bed_for(type));
}

void WeatherAudio::stop() {
  if (m_playing_id.isEmpty()) {
    return;
  }
  AudioSystem::get_instance().stop_sound(m_playing_id.toStdString());
  m_playing_id.clear();
  m_volume = 0.0F;
}

} // namespace App::Core
