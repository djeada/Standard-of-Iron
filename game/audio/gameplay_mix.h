#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Game::Audio {

enum class MixBus : std::uint8_t {
  Unmixed,
  Music,
  Ambience,
  Combat,
  Voice,
  Interface,
  Economy,
  Weather,
  Environment,
  Alert,
  Count
};
enum class ListeningPreset : std::uint8_t {
  Headphones,
  Speakers,
  Night
};
inline constexpr std::size_t k_mix_bus_count = static_cast<std::size_t>(MixBus::Count);
using MixGains = std::array<float, k_mix_bus_count>;
using MixCounts = std::array<unsigned, k_mix_bus_count>;

inline auto mix_index(MixBus bus) -> std::size_t {
  return static_cast<std::size_t>(bus);
}

// Resolve semantic cue names before falling back to the resource category.
inline auto mix_bus_for(std::string_view id, MixBus fallback) -> MixBus {
  if (id.starts_with("sound_")) {
    id.remove_prefix(6);
  }
  if (id.starts_with("sfx.")) {
    id.remove_prefix(4);
  }
  if (id.starts_with("alert.") || id.starts_with("state.")) {
    return MixBus::Alert;
  }
  if (id.starts_with("voice.")) {
    return MixBus::Voice;
  }
  if (id.starts_with("weather.") || id.starts_with("ambience.weather.") ||
      id.starts_with("ambient.weather_")) {
    return MixBus::Weather;
  }
  if (id.starts_with("wildlife.") || id.starts_with("environment.")) {
    return MixBus::Environment;
  }
  if (id.starts_with("build.") || id.starts_with("economy.")) {
    return MixBus::Economy;
  }
  if (id.starts_with("combat.") || id.starts_with("move.") ||
      id.starts_with("movement.")) {
    return MixBus::Combat;
  }
  if (id.starts_with("ui.") || id.starts_with("order.") || id.starts_with("command.")) {
    return MixBus::Interface;
  }
  return fallback;
}

inline auto preset_from_int(int value) -> ListeningPreset {
  return static_cast<ListeningPreset>(std::clamp(value, 0, 2));
}

// Audio-thread-owned. No allocation, locks or wall-clock timing in the callback.
class GameplayMix {
public:
  void prepare(int sample_rate) {
    const float rate = static_cast<float>(std::max(1, sample_rate));
    m_attack = std::exp(-1.0F / (0.020F * rate));
    m_release = std::exp(-1.0F / (0.350F * rate));
    m_gains = base_gains();
    m_targets = m_gains;
  }

  void
  target(const MixCounts& counts, bool voice, bool critical, ListeningPreset preset) {
    m_targets = base_gains();
    const bool night = preset == ListeningPreset::Night;
    // Shared density budget prevents a storm spread across different cue IDs
    // from bypassing per-resource concurrency and cooldown rules.
    const unsigned foreground = counts[mix_index(MixBus::Combat)] +
                                counts[mix_index(MixBus::Economy)] +
                                counts[mix_index(MixBus::Environment)];
    const float density = 1.0F / std::sqrt(std::max(1.0F, float(foreground) / 3.0F));
    for (MixBus bus : {MixBus::Combat, MixBus::Economy, MixBus::Environment}) {
      m_targets[mix_index(bus)] *= density * (night ? 0.70F : 1.0F);
    }
    for (MixBus bus : {MixBus::Music,
                       MixBus::Ambience,
                       MixBus::Weather,
                       MixBus::Combat,
                       MixBus::Economy,
                       MixBus::Environment}) {
      // Routine acknowledgements make a small pocket; critical cues get 4 dB.
      m_targets[mix_index(bus)] *= critical ? 0.63F : (voice ? 0.85F : 1.0F);
    }
    // Two simultaneous voices/alerts must not double the information bus.
    for (MixBus bus : {MixBus::Voice, MixBus::Alert}) {
      m_targets[mix_index(bus)] /=
          std::sqrt(float(std::max(1U, counts[mix_index(bus)])));
    }
  }

  auto next() -> const MixGains& {
    for (std::size_t i = 0; i < k_mix_bus_count; ++i) {
      const float coefficient = m_targets[i] < m_gains[i] ? m_attack : m_release;
      m_gains[i] = m_targets[i] + coefficient * (m_gains[i] - m_targets[i]);
    }
    return m_gains;
  }

  static auto base_gains() -> MixGains {
    // Linear gains: 0, -6, -6, -8, -1, -4, -10, -10, -12, -2 dB.
    return {1.0F, 0.50F, 0.50F, 0.40F, 0.89F, 0.63F, 0.32F, 0.32F, 0.25F, 0.79F};
  }

private:
  MixGains m_gains = base_gains();
  MixGains m_targets = base_gains();
  float m_attack = 0.0F;
  float m_release = 0.0F;
};

} // namespace Game::Audio
