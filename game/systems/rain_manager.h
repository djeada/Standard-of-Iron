#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <utility>

#include "game/map/map_definition.h"

namespace Game::Systems {

enum class RainState {
  Clear,
  FadingIn,
  Active,
  FadingOut
};

struct RainRuntimeState {
  RainState state = RainState::Clear;
  float cycle_time = 0.0F;
  float state_time = 0.0F;
  float intensity = 0.0F;
};

[[nodiscard]] auto parse_rain_state(const QString& value) noexcept -> RainState;
[[nodiscard]] auto rain_state_name(RainState state) noexcept -> const char*;

class RainManager {
public:
  RainManager();
  ~RainManager() = default;

  void configure(const Game::Map::RainSettings& settings, std::uint32_t map_seed);

  void reset();

  [[nodiscard]] auto snapshot() const noexcept -> RainRuntimeState;
  void restore(const RainRuntimeState& state) noexcept;

  void update(float delta_time);

  [[nodiscard]] auto is_enabled() const -> bool { return m_settings.enabled; }

  [[nodiscard]] auto get_state() const -> RainState { return m_state; }

  [[nodiscard]] auto get_intensity() const -> float { return m_current_intensity; }

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

  [[nodiscard]] auto get_wind_direction_deg() const -> float {
    return m_settings.wind_direction_deg;
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
