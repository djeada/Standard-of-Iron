#include "rain_manager.h"

#include <algorithm>

namespace Game::Systems {

auto parse_rain_state(const QString& value) noexcept -> RainState {
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("fading_in")) {
    return RainState::FadingIn;
  }
  if (normalized == QStringLiteral("active")) {
    return RainState::Active;
  }
  if (normalized == QStringLiteral("fading_out")) {
    return RainState::FadingOut;
  }
  return RainState::Clear;
}

auto rain_state_name(RainState state) noexcept -> const char* {
  switch (state) {
  case RainState::Clear:
    return "clear";
  case RainState::FadingIn:
    return "fading_in";
  case RainState::Active:
    return "active";
  case RainState::FadingOut:
    return "fading_out";
  }
  return "clear";
}

RainManager::RainManager() = default;

void RainManager::configure(const Game::Map::RainSettings& settings,
                            std::uint32_t map_seed) {
  m_settings = settings;
  m_seed = map_seed;
  reset();
}

void RainManager::reset() {
  m_state = RainState::Clear;
  m_current_intensity = 0.0F;
  m_state_time = 0.0F;

  if (m_settings.enabled && m_seed != 0 && m_settings.cycle_duration >= 1.0F) {
    const auto cycle_ms =
        static_cast<std::uint32_t>(std::max(1.0F, m_settings.cycle_duration) * 1000.0F);
    m_cycle_time = static_cast<float>(m_seed % cycle_ms) / 1000.0F;
  } else {
    m_cycle_time = 0.0F;
  }
}

auto RainManager::snapshot() const noexcept -> RainRuntimeState {
  return {.state = m_state,
          .cycle_time = m_cycle_time,
          .state_time = m_state_time,
          .intensity = m_current_intensity};
}

void RainManager::restore(const RainRuntimeState& state) noexcept {
  const float cycle_duration = std::max(0.001F, m_settings.cycle_duration);
  m_state = state.state;
  m_cycle_time = std::clamp(state.cycle_time, 0.0F, cycle_duration);
  m_state_time = std::max(0.0F, state.state_time);
  m_current_intensity = std::clamp(state.intensity, 0.0F, 1.0F);
  if (!m_settings.enabled) {
    m_state = RainState::Clear;
    m_current_intensity = 0.0F;
  }
}

void RainManager::update(float delta_time) {
  if (!m_settings.enabled) {
    return;
  }

  m_cycle_time += delta_time;

  if (m_cycle_time >= m_settings.cycle_duration) {
    m_cycle_time -= m_settings.cycle_duration;
  }

  const float rain_start = 0.0F;
  const float rain_end = m_settings.active_duration;
  const float effective_fade =
      std::min(m_settings.fade_duration, m_settings.active_duration / 2.0F);
  const float fade_in_end = rain_start + effective_fade;
  const float fade_out_start = std::max(fade_in_end, rain_end - effective_fade);

  RainState target_state = RainState::Clear;

  if (m_cycle_time >= rain_start && m_cycle_time < rain_end) {
    if (m_cycle_time < fade_in_end) {
      target_state = RainState::FadingIn;
    } else if (m_cycle_time >= fade_out_start) {
      target_state = RainState::FadingOut;
    } else {
      target_state = RainState::Active;
    }
  } else {
    target_state = RainState::Clear;
  }

  if (target_state != m_state) {
    transition_to(target_state);

    if (target_state == RainState::FadingIn) {
      m_state_time = m_cycle_time - rain_start;
    } else if (target_state == RainState::FadingOut) {
      m_state_time = m_cycle_time - fade_out_start;
    } else if (target_state == RainState::Active) {
      m_state_time = m_cycle_time - fade_in_end;
    }
  } else {
    m_state_time += delta_time;
  }

  update_intensity(delta_time);
}

void RainManager::transition_to(RainState new_state) {
  m_state = new_state;
  m_state_time = 0.0F;

  if (m_state_callback) {
    m_state_callback(new_state);
  }
}

void RainManager::update_intensity(float) {
  switch (m_state) {
  case RainState::Clear:
    m_current_intensity = 0.0F;
    break;

  case RainState::FadingIn: {
    const float progress =
        std::min(1.0F, m_state_time / std::max(0.001F, m_settings.fade_duration));
    m_current_intensity = progress * m_settings.intensity;
    break;
  }

  case RainState::Active:
    m_current_intensity = m_settings.intensity;
    break;

  case RainState::FadingOut: {
    const float progress =
        std::min(1.0F, m_state_time / std::max(0.001F, m_settings.fade_duration));
    m_current_intensity = (1.0F - progress) * m_settings.intensity;
    break;
  }
  }
}

} // namespace Game::Systems
