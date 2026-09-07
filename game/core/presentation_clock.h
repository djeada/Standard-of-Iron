#pragma once

#include <algorithm>
#include <cstdint>

namespace Engine::Core {

inline constexpr float k_presentation_max_extrapolation = 1.0F;

inline constexpr float k_presentation_teleport_threshold = 2.0F;

class PresentationClock {
public:
  void reset() {
    m_seconds = 0.0F;
    m_previous_tick_seconds = 0.0F;
    m_seen_sequence = 0U;
    m_seeded = false;
  }

  [[nodiscard]] auto age_seconds() const -> float { return m_seconds; }

  template <typename SampleT>
  auto advance(const SampleT& sample, float frame_delta_seconds) -> float {
    float const tick = sample.tick_seconds;
    if (!m_seeded || sample.snap || tick <= 0.0F) {
      m_seeded = true;
      m_seen_sequence = sample.tick_sequence;
      m_previous_tick_seconds = tick;
      m_seconds = tick;
      return m_seconds;
    }

    float next = m_seconds + std::max(0.0F, frame_delta_seconds);
    if (sample.tick_sequence != m_seen_sequence) {
      auto const published = static_cast<float>(sample.tick_sequence - m_seen_sequence);
      m_seen_sequence = sample.tick_sequence;

      next -= m_previous_tick_seconds * published;
      m_previous_tick_seconds = tick;
    }
    m_seconds =
        std::clamp(next, 0.0F, tick * (1.0F + k_presentation_max_extrapolation));
    return m_seconds;
  }

private:
  float m_seconds{0.0F};
  float m_previous_tick_seconds{0.0F};
  std::uint32_t m_seen_sequence{0U};
  bool m_seeded{false};
};

} // namespace Engine::Core
