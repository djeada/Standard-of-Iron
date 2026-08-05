#pragma once

#include <cstddef>

namespace Game::Audio {

struct LoopSeamReport {

  std::size_t loop_frames = 0;
  std::size_t fade_frames = 0;

  float step_before = 0.0F;
  float step_after = 0.0F;
};

inline constexpr float k_loop_fade_seconds = 0.12F;

inline constexpr float k_max_loop_fade_fraction = 0.1F;

auto seal_loop(float* pcm,
               std::size_t frames,
               unsigned channels,
               unsigned sample_rate) -> LoopSeamReport;

} // namespace Game::Audio
