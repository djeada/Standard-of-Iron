#include "environment_lighting.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

namespace Game::Map {

namespace {

struct LightingKeyframe {
  float hour;
  EnvironmentLightingState lighting;
};

auto make_keyframe(float hour,
                   QVector3D direction,
                   QVector3D primary,
                   float primary_intensity,
                   QVector3D sky,
                   QVector3D bounce,
                   float ambient,
                   QVector3D fog,
                   float fog_density,
                   QVector3D shadow_tint,
                   float shadow_strength,
                   float shadow_softness,
                   float exposure) -> LightingKeyframe {
  EnvironmentLightingState value;
  value.primary_direction = direction.normalized();
  value.primary_color = primary;
  value.primary_intensity = primary_intensity;
  value.sky_color = sky;
  value.ground_bounce_color = bounce;
  value.ambient_intensity = ambient;
  value.fog_color = fog;
  value.fog_density = fog_density;
  value.shadow_tint = shadow_tint;
  value.shadow_strength = shadow_strength;
  value.shadow_softness = shadow_softness;
  value.exposure = exposure;
  return {hour, value};
}

auto mediterranean_summer_profile() -> const std::array<LightingKeyframe, 8>& {
  static const std::array<LightingKeyframe, 8> keyframes{
      make_keyframe(0.0F,
                    {-0.28F, 0.42F, 0.50F},
                    {0.40F, 0.54F, 0.92F},
                    0.34F,
                    {0.14F, 0.23F, 0.48F},
                    {0.15F, 0.20F, 0.36F},
                    0.32F,
                    {0.11F, 0.18F, 0.34F},
                    0.004F,
                    {0.24F, 0.30F, 0.50F},
                    0.42F,
                    0.72F,
                    1.00F),
      make_keyframe(5.0F,
                    {0.72F, 0.10F, 0.18F},
                    {0.94F, 0.49F, 0.30F},
                    0.30F,
                    {0.21F, 0.27F, 0.40F},
                    {0.24F, 0.20F, 0.19F},
                    0.26F,
                    {0.30F, 0.34F, 0.43F},
                    0.012F,
                    {0.22F, 0.24F, 0.34F},
                    0.48F,
                    0.64F,
                    0.88F),
      make_keyframe(7.0F,
                    {0.60F, 0.30F, 0.20F},
                    {1.00F, 0.68F, 0.42F},
                    0.78F,
                    {0.43F, 0.59F, 0.78F},
                    {0.30F, 0.22F, 0.15F},
                    0.22F,
                    {0.56F, 0.65F, 0.74F},
                    0.010F,
                    {0.26F, 0.29F, 0.38F},
                    0.62F,
                    0.46F,
                    0.95F),
      make_keyframe(13.0F,
                    {0.35F, 0.85F, 0.42F},
                    {1.00F, 0.95F, 0.86F},
                    1.00F,
                    {0.50F, 0.64F, 0.82F},
                    {0.28F, 0.23F, 0.17F},
                    0.30F,
                    {0.62F, 0.70F, 0.78F},
                    0.002F,
                    {0.34F, 0.38F, 0.48F},
                    0.58F,
                    0.34F,
                    1.04F),
      make_keyframe(17.0F,
                    {0.74F, 0.24F, 0.32F},
                    {1.00F, 0.66F, 0.34F},
                    1.30F,
                    {0.66F, 0.59F, 0.52F},
                    {0.50F, 0.34F, 0.18F},
                    0.36F,
                    {0.94F, 0.74F, 0.48F},
                    0.005F,
                    {0.34F, 0.35F, 0.50F},
                    0.66F,
                    0.30F,
                    1.10F),
      make_keyframe(20.0F,
                    {-0.58F, 0.16F, 0.30F},
                    {0.92F, 0.44F, 0.29F},
                    0.50F,
                    {0.24F, 0.25F, 0.38F},
                    {0.24F, 0.19F, 0.18F},
                    0.28F,
                    {0.31F, 0.32F, 0.40F},
                    0.008F,
                    {0.18F, 0.20F, 0.30F},
                    0.55F,
                    0.54F,
                    0.86F),
      make_keyframe(22.0F,
                    {-0.20F, 0.35F, 0.50F},
                    {0.42F, 0.56F, 0.94F},
                    0.38F,
                    {0.15F, 0.24F, 0.50F},
                    {0.16F, 0.21F, 0.38F},
                    0.32F,
                    {0.12F, 0.19F, 0.36F},
                    0.005F,
                    {0.25F, 0.31F, 0.52F},
                    0.42F,
                    0.74F,
                    1.00F),
      make_keyframe(24.0F,
                    {-0.28F, 0.42F, 0.50F},
                    {0.40F, 0.54F, 0.92F},
                    0.34F,
                    {0.14F, 0.23F, 0.48F},
                    {0.15F, 0.20F, 0.36F},
                    0.32F,
                    {0.11F, 0.18F, 0.34F},
                    0.004F,
                    {0.24F, 0.30F, 0.50F},
                    0.42F,
                    0.72F,
                    1.00F),
  };
  return keyframes;
}

auto iron_sepulcher_profile() -> const std::array<LightingKeyframe, 8>& {
  static const std::array<LightingKeyframe, 8> keyframes = [] {
    auto result = mediterranean_summer_profile();
    for (auto& keyframe : result) {
      keyframe.lighting.sky_color *= QVector3D(0.70F, 0.80F, 0.94F);
      keyframe.lighting.ground_bounce_color *= QVector3D(0.72F, 0.78F, 0.92F);
      keyframe.lighting.fog_color =
          (keyframe.lighting.fog_color * 0.68F) + QVector3D(0.10F, 0.13F, 0.18F);
      keyframe.lighting.primary_color =
          (keyframe.lighting.primary_color * 0.86F) + QVector3D(0.05F, 0.06F, 0.09F);
      const QVector3D grave_shadow(0.26F, 0.24F, 0.40F);
      keyframe.lighting.shadow_tint +=
          (grave_shadow - keyframe.lighting.shadow_tint) * 0.55F;
      keyframe.lighting.exposure *= 0.92F;
    }
    return result;
  }();
  return keyframes;
}

auto profile_for_name(const QString& name) -> const std::array<LightingKeyframe, 8>& {
  if (name.trimmed().compare(QStringLiteral("iron_sepulcher"), Qt::CaseInsensitive) ==
      0) {
    return iron_sepulcher_profile();
  }
  return mediterranean_summer_profile();
}

auto smooth_curve(float value) noexcept -> float {
  const float t = std::clamp(value, 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto shortest_hour_delta(float from, float to) noexcept -> float {
  float delta = normalize_hour(to) - normalize_hour(from);
  if (delta > 12.0F) {
    delta -= 24.0F;
  } else if (delta < -12.0F) {
    delta += 24.0F;
  }
  return delta;
}

auto apply_weather(EnvironmentLightingState state,
                   WeatherLightingInput weather) -> EnvironmentLightingState {
  const float rain = std::clamp(weather.rain, 0.0F, 1.0F);
  const float snow = std::clamp(weather.snow, 0.0F, 1.0F);
  const float storm = std::clamp(weather.storm, 0.0F, 1.0F);
  const float cloud = std::clamp((rain * 0.72F) + (storm * 0.95F), 0.0F, 1.0F);

  state.cloud_cover = cloud;
  state.wetness = rain;
  state.primary_intensity *= 1.0F - (cloud * 0.48F);
  state.shadow_strength *= 1.0F - (cloud * 0.58F);
  state.shadow_softness =
      std::clamp(state.shadow_softness + (cloud * 0.46F), 0.0F, 1.0F);
  state.ambient_intensity *= 1.0F - (storm * 0.24F);
  state.exposure *= 1.0F - (storm * 0.16F);
  state.fog_density += (rain * 0.006F) + (storm * 0.010F);

  const QVector3D rain_tint(0.42F, 0.50F, 0.60F);
  state.primary_color =
      state.primary_color + ((rain_tint - state.primary_color) * (rain * 0.32F));
  state.sky_color = state.sky_color + ((rain_tint - state.sky_color) * (cloud * 0.42F));
  state.fog_color =
      state.fog_color + ((state.sky_color - state.fog_color) * (0.45F + cloud * 0.3F));

  if (snow > 0.0F) {
    const QVector3D snow_bounce(0.72F, 0.76F, 0.80F);
    state.ground_bounce_color +=
        (snow_bounce - state.ground_bounce_color) * (snow * 0.55F);
    state.ambient_intensity *= 1.0F + (snow * 0.40F);
    state.exposure *= 1.0F + (snow * 0.10F);

    const QVector3D snow_haze(0.76F, 0.83F, 0.94F);
    state.fog_color += (snow_haze - state.fog_color) * (snow * 0.50F);
    state.sky_color += (snow_haze - state.sky_color) * (snow * 0.30F);
    state.fog_density += snow * 0.005F;

    const QVector3D snow_shadow(0.48F, 0.57F, 0.76F);
    state.shadow_tint += (snow_shadow - state.shadow_tint) * (snow * 0.65F);
    state.shadow_strength *= 1.0F - (snow * 0.34F);
    state.shadow_softness =
        std::clamp(state.shadow_softness + (snow * 0.28F), 0.0F, 1.0F);
  }
  return state.sanitized();
}

} // namespace

auto normalize_hour(float hour) noexcept -> float {
  if (!std::isfinite(hour)) {
    return 13.0F;
  }
  float result = std::fmod(hour, 24.0F);
  if (result < 0.0F) {
    result += 24.0F;
  }
  return result;
}

auto hour_for_time_of_day(TimeOfDay time_of_day) noexcept -> float {
  switch (time_of_day) {
  case TimeOfDay::Morning:
    return 7.0F;
  case TimeOfDay::Day:
    return 13.0F;
  case TimeOfDay::Afternoon:
    return 17.0F;
  case TimeOfDay::Night:
    return 22.0F;
  }
  return 13.0F;
}

auto time_of_day_for_hour(float hour) noexcept -> TimeOfDay {
  const float normalized = normalize_hour(hour);
  if (normalized >= 5.0F && normalized < 10.0F) {
    return TimeOfDay::Morning;
  }
  if (normalized >= 10.0F && normalized < 15.5F) {
    return TimeOfDay::Day;
  }
  if (normalized >= 15.5F && normalized < 20.0F) {
    return TimeOfDay::Afternoon;
  }
  return TimeOfDay::Night;
}

auto parse_time_mode(const QString& value) noexcept -> TimeMode {
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("scripted")) {
    return TimeMode::Scripted;
  }
  if (normalized == QStringLiteral("continuous")) {
    return TimeMode::Continuous;
  }
  return TimeMode::Locked;
}

auto time_mode_name(TimeMode mode) noexcept -> const char* {
  switch (mode) {
  case TimeMode::Locked:
    return "locked";
  case TimeMode::Scripted:
    return "scripted";
  case TimeMode::Continuous:
    return "continuous";
  }
  return "locked";
}

auto lighting_for_hour(float hour,
                       const QString& profile,
                       WeatherLightingInput weather) -> EnvironmentLightingState {
  const float normalized = normalize_hour(hour);
  const auto& keyframes = profile_for_name(profile);

  const LightingKeyframe* from = &keyframes.front();
  const LightingKeyframe* to = &keyframes.back();
  for (std::size_t i = 1; i < keyframes.size(); ++i) {
    if (normalized <= keyframes[i].hour) {
      from = &keyframes[i - 1];
      to = &keyframes[i];
      break;
    }
  }

  const float span = std::max(0.001F, to->hour - from->hour);
  const float amount = smooth_curve((normalized - from->hour) / span);
  return apply_weather(
      Render::interpolate_environment_lighting(from->lighting, to->lighting, amount),
      weather);
}

auto lighting_for_time_of_day(TimeOfDay time_of_day) -> EnvironmentLightingState {
  return lighting_for_hour(hour_for_time_of_day(time_of_day));
}

auto time_of_day_name(TimeOfDay time_of_day) noexcept -> const char* {
  switch (time_of_day) {
  case TimeOfDay::Morning:
    return "Morning";
  case TimeOfDay::Day:
    return "Day";
  case TimeOfDay::Afternoon:
    return "Afternoon";
  case TimeOfDay::Night:
    return "Night";
  }
  return "Day";
}

auto representative_clock_time(TimeOfDay time_of_day) noexcept -> const char* {
  switch (time_of_day) {
  case TimeOfDay::Morning:
    return "07:00";
  case TimeOfDay::Day:
    return "13:00";
  case TimeOfDay::Afternoon:
    return "17:00";
  case TimeOfDay::Night:
    return "22:00";
  }
  return "13:00";
}

EnvironmentClock::EnvironmentClock(const EnvironmentDefinition& definition) {
  reset(definition);
}

void EnvironmentClock::reset(const EnvironmentDefinition& definition) {
  m_definition = definition;
  m_definition.start_time = normalize_hour(m_definition.start_time);
  m_definition.day_length_seconds = std::max(1.0F, m_definition.day_length_seconds);
  m_hour = m_definition.start_time;
  m_transition_start_hour = m_hour;
  m_transition_target_hour = m_hour;
  m_transition_duration = 0.0F;
  m_transition_elapsed = 0.0F;
  m_transition_active = false;
}

void EnvironmentClock::update(float simulation_seconds,
                              bool simulation_paused) noexcept {
  if (simulation_paused || simulation_seconds <= 0.0F) {
    return;
  }

  if (m_transition_active) {
    m_transition_elapsed =
        std::min(m_transition_duration, m_transition_elapsed + simulation_seconds);
    const float linear = m_transition_duration > 0.0F
                             ? m_transition_elapsed / m_transition_duration
                             : 1.0F;
    const float amount = smooth_curve(linear);
    m_hour = normalize_hour(
        m_transition_start_hour +
        (shortest_hour_delta(m_transition_start_hour, m_transition_target_hour) *
         amount));
    if (m_transition_elapsed >= m_transition_duration) {
      m_hour = normalize_hour(m_transition_target_hour);
      m_transition_active = false;
    }
    return;
  }

  if (m_definition.time_mode == TimeMode::Continuous) {
    const float hours_per_second = 24.0F / m_definition.day_length_seconds;
    m_hour = normalize_hour(m_hour + (simulation_seconds * hours_per_second));
  }
}

void EnvironmentClock::begin_transition(float target_hour,
                                        float duration_seconds) noexcept {
  m_transition_start_hour = m_hour;
  m_transition_target_hour = normalize_hour(target_hour);
  m_transition_duration = std::max(0.0F, duration_seconds);
  m_transition_elapsed = 0.0F;
  m_transition_active = m_transition_duration > 0.0F;
  if (!m_transition_active) {
    m_hour = m_transition_target_hour;
  }
}

auto EnvironmentClock::snapshot() const noexcept -> EnvironmentClockState {
  return {.hour = m_hour,
          .transition_start_hour = m_transition_start_hour,
          .transition_target_hour = m_transition_target_hour,
          .transition_duration = m_transition_duration,
          .transition_elapsed = m_transition_elapsed,
          .transition_active = m_transition_active};
}

void EnvironmentClock::restore(const EnvironmentDefinition& definition,
                               const EnvironmentClockState& state) noexcept {
  m_definition = definition;
  m_definition.start_time = normalize_hour(m_definition.start_time);
  m_definition.day_length_seconds = std::max(1.0F, m_definition.day_length_seconds);
  m_hour = normalize_hour(state.hour);
  m_transition_start_hour = normalize_hour(state.transition_start_hour);
  m_transition_target_hour = normalize_hour(state.transition_target_hour);
  m_transition_duration = std::max(0.0F, state.transition_duration);
  m_transition_elapsed =
      std::clamp(state.transition_elapsed, 0.0F, m_transition_duration);
  m_transition_active =
      state.transition_active && m_transition_elapsed < m_transition_duration;
}

} // namespace Game::Map
