#pragma once

#include "game/map/map_definition.h"
#include <cstdint>
#include <functional>

namespace Game::Systems {

enum class RainState { Clear, FadingIn, Active, FadingOut };

class RainManager {
public:
  RainManager();
  ~RainManager() = default;

  void configure(const Game::Map::RainSettings &settings,
                 std::uint32_t map_seed);

  void reset();

  void update(float delta_time);

  [[nodiscard]] auto is_enabled() const -> bool { return m_settings.enabled; }

  [[nodiscard]] auto get_state() const -> RainState { return m_state; }

  [[nodiscard]] auto get_intensity() const -> float {
    return m_current_intensity;
  }

  [[nodiscard]] auto get_cycle_time() const -> float { return m_cycle_time; }

  [[nodiscard]] auto get_cycle_duration() const -> float {
    return m_settings.cycle_duration;
  }

  [[nodiscard]] auto is_raining() const -> bool {
    return m_state == RainState::Active || m_state == RainState::FadingIn ||
           m_state == RainState::FadingOut;
  }

  [[nodiscard]] auto get_weather_type() const -> Game::Map::WeatherType {
    return m_settings.type;
  }

  [[nodiscard]] auto get_wind_strength() const -> float {
    return m_settings.wind_strength;
  }

  using StateChangeCallback = std::function<void(RainState new_state)>;
  void set_state_change_callback(StateChangeCallback callback) {
    m_state_callback = std::move(callback);
  }

private:
  void transition_to(RainState new_state);
  void update_intensity(float delta_time);

  Game::Map::RainSettings m_settings;
  std::uint32_t m_seed = 0;
  RainState m_state = RainState::Clear;
  float m_cycle_time = 0.0F;
  float m_state_time = 0.0F;
  float m_current_intensity = 0.0F;
  StateChangeCallback m_state_callback;
};

} // namespace Game::Systems
