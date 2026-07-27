#pragma once

#include <QString>

#include "scene/environment_lighting.h"

namespace Game::Map {

using EnvironmentLightingState = Render::EnvironmentLightingState;

enum class TimeOfDay {
  Morning,
  Day,
  Afternoon,
  Night
};

enum class TimeMode {
  Locked,
  Scripted,
  Continuous
};

struct EnvironmentDefinition {
  float start_time = 13.0F;
  TimeMode time_mode = TimeMode::Locked;
  float day_length_seconds = 1800.0F;
  QString lighting_profile = QStringLiteral("mediterranean_summer");
  float fog_density_override = -1.0F;
  float exposure_override = -1.0F;
};

struct WeatherLightingInput {
  float rain = 0.0F;
  float snow = 0.0F;
  float storm = 0.0F;
};

struct EnvironmentClockState {
  float hour = 13.0F;
  float transition_start_hour = 13.0F;
  float transition_target_hour = 13.0F;
  float transition_duration = 0.0F;
  float transition_elapsed = 0.0F;
  bool transition_active = false;
};

[[nodiscard]] auto normalize_hour(float hour) noexcept -> float;
[[nodiscard]] auto hour_for_time_of_day(TimeOfDay time_of_day) noexcept -> float;
[[nodiscard]] auto time_of_day_for_hour(float hour) noexcept -> TimeOfDay;
[[nodiscard]] auto parse_time_mode(const QString& value) noexcept -> TimeMode;
[[nodiscard]] auto time_mode_name(TimeMode mode) noexcept -> const char*;
[[nodiscard]] auto
lighting_for_hour(float hour,
                  const QString& profile = QStringLiteral("mediterranean_summer"),
                  WeatherLightingInput weather = {}) -> EnvironmentLightingState;
[[nodiscard]] auto
lighting_for_time_of_day(TimeOfDay time_of_day) -> EnvironmentLightingState;

[[nodiscard]] auto time_of_day_name(TimeOfDay time_of_day) noexcept -> const char*;
[[nodiscard]] auto representative_clock_time(TimeOfDay time_of_day) noexcept -> const
    char*;

class EnvironmentClock {
public:
  EnvironmentClock() = default;
  explicit EnvironmentClock(const EnvironmentDefinition& definition);

  void reset(const EnvironmentDefinition& definition);
  void update(float simulation_seconds, bool simulation_paused) noexcept;
  void begin_transition(float target_hour, float duration_seconds) noexcept;

  [[nodiscard]] auto hour() const noexcept -> float { return m_hour; }
  [[nodiscard]] auto transition_active() const noexcept -> bool {
    return m_transition_active;
  }
  [[nodiscard]] auto definition() const -> const EnvironmentDefinition& {
    return m_definition;
  }
  [[nodiscard]] auto snapshot() const noexcept -> EnvironmentClockState;
  void restore(const EnvironmentDefinition& definition,
               const EnvironmentClockState& state) noexcept;
  [[nodiscard]] auto
  lighting(WeatherLightingInput weather = {}) const -> EnvironmentLightingState {
    EnvironmentLightingState result =
        lighting_for_hour(m_hour, m_definition.lighting_profile, weather);
    if (m_definition.fog_density_override >= 0.0F) {
      result.fog_density = m_definition.fog_density_override;
    }
    if (m_definition.exposure_override >= 0.0F) {
      result.exposure = m_definition.exposure_override;
    }
    return result.sanitized();
  }

private:
  EnvironmentDefinition m_definition;
  float m_hour = 13.0F;
  float m_transition_start_hour = 13.0F;
  float m_transition_target_hour = 13.0F;
  float m_transition_duration = 0.0F;
  float m_transition_elapsed = 0.0F;
  bool m_transition_active = false;
};

} // namespace Game::Map
