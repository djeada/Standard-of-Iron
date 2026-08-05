#pragma once

#include <QString>

#include "game/map/map_definition.h"

namespace Game::Systems {
class RainManager;
}

namespace App::Core {

class WeatherAudio {
public:
  static constexpr float k_silence_threshold = 0.02F;

  static constexpr float k_full_intensity_volume = 0.85F;

  static constexpr float k_volume_epsilon = 0.02F;

  void preload(Game::Map::WeatherType type);

  void update(const Game::Systems::RainManager* rain);
  void stop();

  [[nodiscard]] auto playing_id() const -> const QString& { return m_playing_id; }
  [[nodiscard]] auto volume() const -> float { return m_volume; }

  [[nodiscard]] static auto bed_for(Game::Map::WeatherType type) -> QString;
  [[nodiscard]] static auto volume_for(float intensity) -> float;

private:
  void start(const QString& bed_id, float volume);

  QString m_playing_id;
  float m_volume = 0.0F;
};

} // namespace App::Core
