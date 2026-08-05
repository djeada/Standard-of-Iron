#include "loop_seam.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Game::Audio {

namespace {

auto wrap_step(const float* pcm,
               std::size_t frames,
               unsigned channels,
               std::size_t loop_frames) -> float {
  if (pcm == nullptr || frames == 0 || loop_frames == 0 || channels == 0) {
    return 0.0F;
  }
  const std::size_t last = (loop_frames - 1) * channels;
  float worst = 0.0F;
  for (unsigned channel = 0; channel < channels; ++channel) {
    worst = std::max(worst, std::abs(pcm[channel] - pcm[last + channel]));
  }
  return worst;
}

} // namespace

auto seal_loop(float* pcm,
               std::size_t frames,
               unsigned channels,
               unsigned sample_rate) -> LoopSeamReport {
  LoopSeamReport report;
  if (pcm == nullptr || channels == 0 || sample_rate == 0 || frames == 0) {
    return report;
  }

  report.step_before = wrap_step(pcm, frames, channels, frames);

  const auto requested =
      static_cast<std::size_t>(k_loop_fade_seconds * static_cast<float>(sample_rate));
  const auto ceiling =
      static_cast<std::size_t>(k_max_loop_fade_fraction * static_cast<float>(frames));
  const std::size_t fade = std::min(requested, ceiling);
  if (fade < 2) {
    return report;
  }

  const std::size_t loop_frames = frames - fade;

  for (std::size_t k = 0; k < fade; ++k) {
    const float position = static_cast<float>(k) / static_cast<float>(fade) * 0.5F *
                           std::numbers::pi_v<float>;
    const float head_gain = std::sin(position);
    const float tail_gain = std::cos(position);
    float* head = pcm + k * channels;
    const float* tail = pcm + (loop_frames + k) * channels;
    for (unsigned channel = 0; channel < channels; ++channel) {
      head[channel] = head[channel] * head_gain + tail[channel] * tail_gain;
    }
  }

  report.loop_frames = loop_frames;
  report.fade_frames = fade;
  report.step_after = wrap_step(pcm, frames, channels, loop_frames);
  return report;
}

} // namespace Game::Audio
