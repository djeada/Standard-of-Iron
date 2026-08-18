#include "resampler.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <utility>

namespace Game::Audio {

namespace {

auto bessel_i0(double x) -> double {
  double sum = 1.0;
  double term = 1.0;
  for (int k = 1; k < 60; ++k) {
    term *= (x * 0.5) / static_cast<double>(k);
    const double contribution = term * term;
    sum += contribution;
    if (contribution < sum * 1.0e-16) {
      break;
    }
  }
  return sum;
}

auto sinc(double x) -> double {
  if (std::abs(x) < 1.0e-12) {
    return 1.0;
  }
  const double pix = std::numbers::pi * x;
  return std::sin(pix) / pix;
}

auto design_filter(std::size_t taps, double cutoff, double beta) -> std::vector<float> {
  std::vector<float> filter(taps, 0.0F);
  const double centre = static_cast<double>(taps - 1) * 0.5;
  const double denominator = bessel_i0(beta);
  for (std::size_t k = 0; k < taps; ++k) {
    const double offset = static_cast<double>(k) - centre;
    const double ratio = offset / centre;
    const double window =
        bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / denominator;
    filter[k] = static_cast<float>(2.0 * cutoff * sinc(2.0 * cutoff * offset) * window);
  }
  return filter;
}

auto channels_are_identical(const std::vector<float>& pcm, unsigned channels) -> bool {
  if (channels < 2) {
    return true;
  }
  for (std::size_t index = 0; index + channels <= pcm.size(); index += channels) {
    for (unsigned channel = 1; channel < channels; ++channel) {
      if (pcm[index] != pcm[index + channel]) {
        return false;
      }
    }
  }
  return true;
}

} // namespace

auto resample_to(std::vector<float>& pcm,
                 unsigned channels,
                 unsigned rate_in,
                 unsigned rate_out) -> ResampleReport {
  ResampleReport report;
  report.rate_in = rate_in;
  report.rate_out = rate_out;

  if (channels == 0 || rate_in == 0 || rate_out == 0 || pcm.empty()) {
    return report;
  }

  const std::size_t frames_in = pcm.size() / channels;
  if (frames_in == 0) {
    return report;
  }

  const unsigned divisor = std::gcd(rate_in, rate_out);
  const unsigned up = rate_out / divisor;
  const unsigned down = rate_in / divisor;
  report.up = up;
  report.down = down;
  report.frames_out = frames_in;
  if (up == 1 && down == 1) {
    return report;
  }

  const double intermediate = static_cast<double>(rate_in) * static_cast<double>(up);
  const double passband = 0.5 * static_cast<double>(std::min(rate_in, rate_out));
  const double cutoff = passband / intermediate;
  const double transition =
      passband * static_cast<double>(k_resampler_transition_fraction) / intermediate;
  const double beta = 0.1102 * (static_cast<double>(k_resampler_stopband_db) - 8.7);

  auto taps = static_cast<std::size_t>(
      std::ceil((static_cast<double>(k_resampler_stopband_db) - 8.0) /
                (2.285 * 2.0 * std::numbers::pi * transition)));
  taps = std::max<std::size_t>(taps, 8);
  const std::size_t taps_per_phase = (taps + up - 1) / up;
  taps = taps_per_phase * up;
  report.taps_per_phase = taps_per_phase;

  const std::vector<float> filter = design_filter(taps, cutoff, beta);
  const std::size_t centre = (taps - 1) / 2;

  const std::size_t frames_out = (frames_in * up) / down;
  if (frames_out == 0) {
    return report;
  }

  const bool mono_content = channels_are_identical(pcm, channels);
  const unsigned worked_channels = mono_content ? 1U : channels;

  std::vector<float> out(frames_out * channels, 0.0F);
  for (std::size_t frame = 0; frame < frames_out; ++frame) {
    const std::size_t numerator = frame * down + centre;
    const std::size_t phase = numerator % up;
    const std::size_t base = (numerator - phase) / up;
    const std::size_t first_tap =
        base >= frames_in ? (base - frames_in) + 1U : std::size_t{0};
    const std::size_t last_tap = std::min(taps_per_phase - 1, base);
    for (unsigned channel = 0; channel < worked_channels; ++channel) {
      float total = 0.0F;
      const float* source = pcm.data() + (base - first_tap) * channels + channel;
      const float* tap_weight = filter.data() + phase + first_tap * up;
      for (std::size_t tap = first_tap; tap <= last_tap; ++tap) {
        total += *tap_weight * *source;
        tap_weight += up;
        source -= channels;
      }
      out[frame * channels + channel] = total * static_cast<float>(up);
    }
    if (mono_content) {
      for (unsigned channel = 1; channel < channels; ++channel) {
        out[frame * channels + channel] = out[frame * channels];
      }
    }
  }

  pcm = std::move(out);
  report.frames_out = frames_out;
  report.applied = true;
  return report;
}

} // namespace Game::Audio
