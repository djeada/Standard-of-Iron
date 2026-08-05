#pragma once

#include <cstddef>
#include <vector>

namespace Game::Audio {

struct ResampleReport {
  unsigned rate_in = 0;
  unsigned rate_out = 0;
  unsigned up = 1;
  unsigned down = 1;
  std::size_t taps_per_phase = 0;
  std::size_t frames_out = 0;
  bool applied = false;
};

inline constexpr float k_resampler_stopband_db = 90.0F;

inline constexpr float k_resampler_transition_fraction = 0.1F;

auto resample_to(std::vector<float>& pcm,
                 unsigned channels,
                 unsigned rate_in,
                 unsigned rate_out) -> ResampleReport;

} // namespace Game::Audio
