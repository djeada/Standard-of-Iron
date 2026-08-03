#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Game::Audio {

class BusLimiter {
public:
  static constexpr float DEFAULT_CEILING = 0.97F;
  static constexpr float DEFAULT_KNEE_DB = 6.0F;
  static constexpr float DEFAULT_LOOKAHEAD_MS = 3.0F;
  static constexpr float DEFAULT_RELEASE_MS = 120.0F;
  static constexpr int MAX_CHANNELS = 2;
  static constexpr float ATTACK_CONVERGENCE = 4.0F;

  void prepare(int sample_rate, int channels) {
    m_channels = std::clamp(channels, 1, MAX_CHANNELS);
    const auto rate = static_cast<float>(std::max(sample_rate, 1));
    m_lookahead = std::max<std::size_t>(
        1, static_cast<std::size_t>(DEFAULT_LOOKAHEAD_MS * 0.001F * rate));
    m_delay.assign(m_lookahead * static_cast<std::size_t>(m_channels), 0.0F);
    m_write = 0;
    m_block_position = 0;
    m_current_block_peak = 0.0F;
    m_previous_block_peak = 0.0F;
    m_gain = 1.0F;
    m_attack =
        std::exp(-ATTACK_CONVERGENCE / std::max(1.0F, static_cast<float>(m_lookahead)));
    m_release = std::exp(-1.0F / std::max(1.0F, DEFAULT_RELEASE_MS * 0.001F * rate));
    m_knee = std::pow(10.0F, -DEFAULT_KNEE_DB / 20.0F);
    m_ready = true;
  }

  void reset() {
    std::fill(m_delay.begin(), m_delay.end(), 0.0F);
    m_write = 0;
    m_block_position = 0;
    m_current_block_peak = 0.0F;
    m_previous_block_peak = 0.0F;
    m_gain = 1.0F;
  }

  [[nodiscard]] auto is_ready() const -> bool { return m_ready; }

  [[nodiscard]] auto gain() const -> float { return m_gain; }

  void process(float* buffer, unsigned frames) {
    if (!m_ready || buffer == nullptr) {
      return;
    }
    const auto channels = static_cast<std::size_t>(m_channels);
    for (unsigned frame = 0; frame < frames; ++frame) {
      float* input = buffer + (static_cast<std::size_t>(frame) * channels);

      float peak = 0.0F;
      for (std::size_t channel = 0; channel < channels; ++channel) {
        peak = std::max(peak, std::abs(input[channel]));
      }
      m_current_block_peak = std::max(m_current_block_peak, peak);
      if (++m_block_position >= m_lookahead) {
        m_previous_block_peak = m_current_block_peak;
        m_current_block_peak = 0.0F;
        m_block_position = 0;
      }

      const float window_peak = std::max(m_current_block_peak, m_previous_block_peak);
      const float target = target_gain(window_peak);
      const float coefficient = target < m_gain ? m_attack : m_release;
      m_gain = target + (coefficient * (m_gain - target));

      const std::size_t slot = m_write * channels;
      for (std::size_t channel = 0; channel < channels; ++channel) {
        const float delayed = m_delay[slot + channel];
        m_delay[slot + channel] = input[channel];
        input[channel] = std::clamp(delayed * m_gain, -m_ceiling, m_ceiling);
      }
      if (++m_write >= m_lookahead) {
        m_write = 0;
      }
    }
  }

private:
  [[nodiscard]] auto target_gain(float peak) const -> float {
    const float knee_start = m_ceiling * m_knee;
    if (peak <= knee_start) {
      return 1.0F;
    }
    if (peak >= m_ceiling) {
      return m_ceiling / peak;
    }
    const float blend = (peak - knee_start) / std::max(m_ceiling - knee_start, 1e-6F);
    const float hard = m_ceiling / peak;
    const float shaped = 1.0F - ((1.0F - hard) * blend * blend);
    return std::min(1.0F, shaped);
  }

  std::vector<float> m_delay;
  std::size_t m_lookahead = 1;
  std::size_t m_write = 0;
  std::size_t m_block_position = 0;
  float m_current_block_peak = 0.0F;
  float m_previous_block_peak = 0.0F;
  float m_gain = 1.0F;
  float m_attack = 0.0F;
  float m_release = 0.0F;
  float m_knee = 0.5F;
  float m_ceiling = DEFAULT_CEILING;
  int m_channels = MAX_CHANNELS;
  bool m_ready = false;
};

} // namespace Game::Audio
