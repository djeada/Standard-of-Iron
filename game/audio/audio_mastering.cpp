#include "audio_mastering.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Game::Audio::Mastering {

namespace {

constexpr float PI = 3.14159265358979323846F;
constexpr float SILENCE = 1e-12F;
constexpr std::size_t MAX_CHANNELS = 8;

constexpr int ANALYSIS_FFT_SIZE = 2048;
constexpr int ANALYSIS_MIN_FFT_SIZE = 256;
constexpr int ANALYSIS_MAX_SPECTRA = 192;
constexpr float ANALYSIS_SMOOTH_OCTAVES = 0.5F;

constexpr float ZONE_BODY_LO_HZ = 200.0F;
constexpr float ZONE_BODY_HI_HZ = 800.0F;
constexpr float ZONE_PRESENCE_LO_HZ = 2000.0F;
constexpr float ZONE_PRESENCE_HI_HZ = 5000.0F;
constexpr float ZONE_AIR_LO_HZ = 6000.0F;
constexpr float ZONE_AIR_HI_HZ = 12000.0F;

constexpr float RIDGE_LO_HZ = 1500.0F;
constexpr float RIDGE_HI_HZ = 12000.0F;
constexpr float RIDGE_HOT_RATIO = 1.9952623F;
constexpr float RIDGE_MIN_HOT_FRACTION = 0.5F;
constexpr float RIDGE_MIN_EXCESS_DB = 5.0F;
constexpr int RIDGE_MIN_SPECTRA = 24;
constexpr float RIDGE_MIN_SECONDS = 20.0F;
constexpr float RIDGE_JOIN_OCTAVES = 1.0F / 6.0F;
constexpr float RIDGE_KEEP_DB = 3.0F;
constexpr float RIDGE_CORRECTION = 0.75F;
constexpr float RIDGE_MIN_CUT_DB = 0.75F;
constexpr float RIDGE_MIN_Q = 3.0F;
constexpr float RIDGE_MAX_Q = 10.0F;
constexpr float RIDGE_CUT_LO_HZ = 3000.0F;
constexpr float RIDGE_CUT_LO_DB = 3.0F;
constexpr float RIDGE_CUT_HI_HZ = 5000.0F;
constexpr float RIDGE_CUT_HI_DB = 7.0F;

constexpr float HARSH_PRESENCE_HZ = 3400.0F;
constexpr float HARSH_PRESENCE_Q = 1.1F;
constexpr float HARSH_PRESENCE_ATTACK_MS = 4.0F;
constexpr float HARSH_PRESENCE_RELEASE_MS = 90.0F;
constexpr float HARSH_AIR_HZ = 7200.0F;
constexpr float HARSH_AIR_Q = 0.9F;
constexpr float HARSH_AIR_ATTACK_MS = 3.0F;
constexpr float HARSH_AIR_RELEASE_MS = 70.0F;
constexpr float HARSH_AIR_THRESHOLD_OFFSET_DB = 2.0F;
constexpr float HARSH_RATIO = 3.0F;
constexpr std::size_t HARSH_CONTROL_SAMPLES = 32;
constexpr float HARSH_GAIN_SMOOTHING = 0.06F;

constexpr float TILT_PRESENCE_HZ = 3200.0F;
constexpr float TILT_PRESENCE_Q = 0.8F;
constexpr float TILT_AIR_HZ = 6500.0F;
constexpr float TILT_MIN_DB = 0.25F;

constexpr float DC_BLOCKER_DECAY = 0.995F;

constexpr std::size_t LOUDNESS_HOPS_PER_BLOCK = 4;
constexpr float LOUDNESS_HOP_SECONDS = 0.1F;
constexpr float LOUDNESS_RELATIVE_GATE_DB = 10.0F;
constexpr float LOUDNESS_OFFSET_DB = 0.691F;
constexpr float MAX_LIMITING_DB = 4.0F;

constexpr float INTERFACE_MAKEUP_DB = 12.0F;

constexpr float VOICE_LOUDNESS_AUTHORITY_DB = 14.0F;

constexpr int LIMITER_BLOCK_SAMPLES = 32;
constexpr float LIMITER_LOOKAHEAD_MS = 4.0F;
constexpr float LIMITER_RELEASE_MS = 120.0F;

auto to_db(float linear) -> float {
  return 20.0F * std::log10(std::max(linear, SILENCE));
}

auto from_db(float decibels) -> float {
  return std::pow(10.0F, decibels / 20.0F);
}

struct Biquad {
  float b0 = 1.0F;
  float b1 = 0.0F;
  float b2 = 0.0F;
  float a1 = 0.0F;
  float a2 = 0.0F;
};

struct BiquadState {
  float x1 = 0.0F;
  float x2 = 0.0F;
  float y1 = 0.0F;
  float y2 = 0.0F;

  auto process(const Biquad& c, float input) -> float {
    const float output =
        (c.b0 * input) + (c.b1 * x1) + (c.b2 * x2) - (c.a1 * y1) - (c.a2 * y2);
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    return output;
  }
};

auto normalise(float b0, float b1, float b2, float a0, float a1, float a2) -> Biquad {
  if (std::abs(a0) < SILENCE) {
    return {};
  }
  return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

auto safe_frequency(float frequency_hz, int sample_rate) -> float {
  const float nyquist = 0.5F * static_cast<float>(sample_rate);
  return std::clamp(frequency_hz, 20.0F, nyquist * 0.9F);
}

auto make_peaking(float frequency_hz,
                  float q,
                  float gain_db,
                  int sample_rate) -> Biquad {
  const float omega = 2.0F * PI * safe_frequency(frequency_hz, sample_rate) /
                      static_cast<float>(sample_rate);
  const float sin_omega = std::sin(omega);
  const float cos_omega = std::cos(omega);
  const float alpha = sin_omega / (2.0F * std::max(q, 0.05F));
  const float amplitude = std::pow(10.0F, gain_db / 40.0F);
  return normalise(1.0F + (alpha * amplitude),
                   -2.0F * cos_omega,
                   1.0F - (alpha * amplitude),
                   1.0F + (alpha / amplitude),
                   -2.0F * cos_omega,
                   1.0F - (alpha / amplitude));
}

auto make_high_shelf(float frequency_hz,
                     float gain_db,
                     int sample_rate,
                     float slope = 0.9F) -> Biquad {
  const float omega = 2.0F * PI * safe_frequency(frequency_hz, sample_rate) /
                      static_cast<float>(sample_rate);
  const float sin_omega = std::sin(omega);
  const float cos_omega = std::cos(omega);
  const float amplitude = std::pow(10.0F, gain_db / 40.0F);
  const float alpha =
      (sin_omega * 0.5F) *
      std::sqrt(((amplitude + (1.0F / amplitude)) * ((1.0F / slope) - 1.0F)) + 2.0F);
  const float edge = 2.0F * std::sqrt(amplitude) * alpha;
  return normalise(
      amplitude * ((amplitude + 1.0F) + ((amplitude - 1.0F) * cos_omega) + edge),
      -2.0F * amplitude * ((amplitude - 1.0F) + ((amplitude + 1.0F) * cos_omega)),
      amplitude * ((amplitude + 1.0F) + ((amplitude - 1.0F) * cos_omega) - edge),
      (amplitude + 1.0F) - ((amplitude - 1.0F) * cos_omega) + edge,
      2.0F * ((amplitude - 1.0F) - ((amplitude + 1.0F) * cos_omega)),
      (amplitude + 1.0F) - ((amplitude - 1.0F) * cos_omega) - edge);
}

auto make_bandpass(float frequency_hz, float q, int sample_rate) -> Biquad {
  const float omega = 2.0F * PI * safe_frequency(frequency_hz, sample_rate) /
                      static_cast<float>(sample_rate);
  const float sin_omega = std::sin(omega);
  const float cos_omega = std::cos(omega);
  const float alpha = sin_omega / (2.0F * std::max(q, 0.05F));
  return normalise(alpha, 0.0F, -alpha, 1.0F + alpha, -2.0F * cos_omega, 1.0F - alpha);
}

auto make_highpass(float frequency_hz, float q, int sample_rate) -> Biquad {
  const float omega = 2.0F * PI * safe_frequency(frequency_hz, sample_rate) /
                      static_cast<float>(sample_rate);
  const float sin_omega = std::sin(omega);
  const float cos_omega = std::cos(omega);
  const float alpha = sin_omega / (2.0F * std::max(q, 0.05F));
  return normalise((1.0F + cos_omega) * 0.5F,
                   -(1.0F + cos_omega),
                   (1.0F + cos_omega) * 0.5F,
                   1.0F + alpha,
                   -2.0F * cos_omega,
                   1.0F - alpha);
}

void fft_in_place(std::vector<std::complex<float>>& data) {
  const std::size_t count = data.size();
  for (std::size_t i = 1, j = 0; i < count; ++i) {
    std::size_t bit = count >> 1U;
    for (; (j & bit) != 0U; bit >>= 1U) {
      j ^= bit;
    }
    j |= bit;
    if (i < j) {
      std::swap(data[i], data[j]);
    }
  }
  for (std::size_t size = 2; size <= count; size <<= 1U) {
    const float angle = -2.0F * PI / static_cast<float>(size);
    const std::complex<float> step(std::cos(angle), std::sin(angle));
    for (std::size_t start = 0; start < count; start += size) {
      std::complex<float> twiddle(1.0F, 0.0F);
      for (std::size_t k = 0; k < size / 2; ++k) {
        const std::complex<float> even = data[start + k];
        const std::complex<float> odd = data[start + k + (size / 2)] * twiddle;
        data[start + k] = even + odd;
        data[start + k + (size / 2)] = even - odd;
        twiddle *= step;
      }
    }
  }
}

void smooth_over_octaves(const std::vector<float>& magnitude,
                         std::vector<double>& prefix,
                         std::vector<float>& smoothed,
                         float octaves,
                         std::size_t first,
                         std::size_t last) {
  const std::size_t count = magnitude.size();
  prefix.assign(count + 1, 0.0);
  for (std::size_t i = 0; i < count; ++i) {
    prefix[i + 1] = prefix[i] + double(magnitude[i]);
  }
  const float lower = std::pow(2.0F, -octaves);
  const float upper = std::pow(2.0F, octaves);
  smoothed.assign(count, 0.0F);
  for (std::size_t k = first; k < last; ++k) {
    const auto index = static_cast<float>(k);
    auto lo = static_cast<std::size_t>(std::max(1.0F, index * lower));
    auto hi = static_cast<std::size_t>(index * upper) + 1;
    hi = std::min(hi, count);
    if (hi <= lo) {
      hi = std::min(count, lo + 1);
    }
    if (hi <= lo) {
      smoothed[k] = magnitude[k];
      continue;
    }
    smoothed[k] = static_cast<float>((prefix[hi] - prefix[lo]) / double(hi - lo));
  }
}

struct LoudnessMeter {
  Biquad shelf;
  Biquad highpass;
  BiquadState shelf_state;
  BiquadState highpass_state;
  std::size_t hop = 1;
  std::size_t hop_position = 0;
  double hop_energy = 0.0;
  std::vector<double> hop_energies;

  void prepare(int sample_rate) {
    shelf = make_high_shelf(1681.97F, 3.999F, sample_rate, 1.0F);
    highpass = make_highpass(38.13F, 0.5F, sample_rate);
    hop = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(LOUDNESS_HOP_SECONDS *
                                 static_cast<float>(sample_rate)));
    hop_position = 0;
    hop_energy = 0.0;
    hop_energies.clear();
  }

  void push(float mono) {
    const float weighted =
        highpass_state.process(highpass, shelf_state.process(shelf, mono));
    hop_energy += double(weighted) * double(weighted);
    if (++hop_position >= hop) {
      hop_energies.push_back(hop_energy);
      hop_energy = 0.0;
      hop_position = 0;
    }
  }

  [[nodiscard]] auto lufs() const -> float {
    const std::size_t span = LOUDNESS_HOPS_PER_BLOCK;
    std::vector<double> powers;
    if (hop_energies.size() >= span) {
      const double window = double(hop * span);
      double running = 0.0;
      for (std::size_t i = 0; i < span; ++i) {
        running += hop_energies[i];
      }
      powers.push_back(running / window);
      for (std::size_t i = span; i < hop_energies.size(); ++i) {
        running += hop_energies[i] - hop_energies[i - span];
        powers.push_back(running / window);
      }
    } else {
      double total = hop_energy;
      for (const double energy : hop_energies) {
        total += energy;
      }
      const double samples = double((hop_energies.size() * hop) + hop_position);
      if (samples <= 0.0) {
        return -70.0F;
      }
      powers.push_back(total / samples);
    }

    double total = 0.0;
    std::size_t counted = 0;
    for (const double power : powers) {
      if (power > 1e-10) {
        total += power;
        ++counted;
      }
    }
    if (counted == 0) {
      return -70.0F;
    }
    const double ungated = total / double(counted);
    const double gate = ungated * std::pow(10.0, -LOUDNESS_RELATIVE_GATE_DB / 10.0);
    double kept = 0.0;
    std::size_t kept_count = 0;
    for (const double power : powers) {
      if (power > 1e-10 && power >= gate) {
        kept += power;
        ++kept_count;
      }
    }
    if (kept_count == 0) {
      kept = total;
      kept_count = counted;
    }
    return static_cast<float>(10.0 * std::log10(kept / double(kept_count))) -
           LOUDNESS_OFFSET_DB;
  }
};

struct Cluster {
  float centre_hz = 0.0F;
  float low_hz = 0.0F;
  float high_hz = 0.0F;
  double weight = 0.0;
  float excess_db = 0.0F;
  float hot_fraction = 0.0F;
};

auto cluster_score(const Cluster& cluster) -> float {
  return cluster.excess_db * cluster.hot_fraction;
}

auto ridge_max_cut_db(float frequency_hz) -> float {
  const float span = RIDGE_CUT_HI_HZ - RIDGE_CUT_LO_HZ;
  const float blend = std::clamp((frequency_hz - RIDGE_CUT_LO_HZ) / span, 0.0F, 1.0F);
  return RIDGE_CUT_LO_DB + (blend * (RIDGE_CUT_HI_DB - RIDGE_CUT_LO_DB));
}

void collect_notches(Analysis& analysis,
                     const std::vector<float>& average,
                     const std::vector<int>& hot,
                     const std::vector<float>& excess,
                     int spectra,
                     float bin_hz,
                     float seconds) {
  if (spectra < RIDGE_MIN_SPECTRA || seconds < RIDGE_MIN_SECONDS) {
    return;
  }
  const auto bins = average.size();
  const auto lo_bin = static_cast<std::size_t>(RIDGE_LO_HZ / bin_hz);
  const auto hi_bin = std::min(bins, static_cast<std::size_t>(RIDGE_HI_HZ / bin_hz));
  const auto frames = static_cast<float>(spectra);

  std::vector<Cluster> clusters;
  std::size_t bin = lo_bin;
  while (bin < hi_bin) {
    const bool marked =
        (static_cast<float>(hot[bin]) / frames >= RIDGE_MIN_HOT_FRACTION) &&
        (excess[bin] / frames >= RIDGE_MIN_EXCESS_DB);
    if (!marked) {
      ++bin;
      continue;
    }
    std::size_t end = bin;
    while (end < hi_bin &&
           (static_cast<float>(hot[end]) / frames >= RIDGE_MIN_HOT_FRACTION) &&
           (excess[end] / frames >= RIDGE_MIN_EXCESS_DB)) {
      ++end;
    }
    Cluster cluster;
    double weight = 0.0;
    double weighted_hz = 0.0;
    float peak_excess = 0.0F;
    float peak_hot = 0.0F;
    for (std::size_t k = bin; k < end; ++k) {
      weight += double(average[k]);
      weighted_hz += double(average[k]) * double(static_cast<float>(k) * bin_hz);
      peak_excess = std::max(peak_excess, excess[k] / frames);
      peak_hot = std::max(peak_hot, static_cast<float>(hot[k]) / frames);
    }
    if (weight <= 0.0) {
      bin = end;
      continue;
    }
    cluster.weight = weight;
    cluster.centre_hz = static_cast<float>(weighted_hz / weight);
    cluster.low_hz = static_cast<float>(bin) * bin_hz;
    cluster.high_hz = static_cast<float>(end) * bin_hz;
    cluster.excess_db = peak_excess;
    cluster.hot_fraction = peak_hot;
    clusters.push_back(cluster);
    bin = end;
  }

  std::vector<Cluster> merged;
  const float join = std::pow(2.0F, RIDGE_JOIN_OCTAVES);
  for (const Cluster& cluster : clusters) {
    if (!merged.empty() && cluster.centre_hz <= merged.back().high_hz * join) {
      Cluster& target = merged.back();
      const double total = target.weight + cluster.weight;
      target.centre_hz =
          static_cast<float>(((double(target.centre_hz) * target.weight) +
                              (double(cluster.centre_hz) * cluster.weight)) /
                             total);
      target.weight = total;
      target.low_hz = std::min(target.low_hz, cluster.low_hz);
      target.high_hz = std::max(target.high_hz, cluster.high_hz);
      target.excess_db = std::max(target.excess_db, cluster.excess_db);
      target.hot_fraction = std::max(target.hot_fraction, cluster.hot_fraction);
      continue;
    }
    merged.push_back(cluster);
  }

  std::sort(merged.begin(), merged.end(), [](const Cluster& a, const Cluster& b) {
    return cluster_score(a) > cluster_score(b);
  });

  for (const Cluster& cluster : merged) {
    if (analysis.notch_count >= MAX_NOTCHES) {
      break;
    }
    const float width = std::max(cluster.high_hz - cluster.low_hz, bin_hz * 2.5F);
    const float cut =
        std::clamp(-(cluster.excess_db - RIDGE_KEEP_DB) * RIDGE_CORRECTION,
                   -ridge_max_cut_db(cluster.centre_hz),
                   0.0F);
    if (cut > -RIDGE_MIN_CUT_DB) {
      continue;
    }
    Notch& notch = analysis.notches[analysis.notch_count];
    notch.frequency_hz = cluster.centre_hz;
    notch.q = std::clamp(cluster.centre_hz / width, RIDGE_MIN_Q, RIDGE_MAX_Q);
    notch.gain_db = cut;
    ++analysis.notch_count;
  }
}

auto choose_fft_size(std::size_t frames) -> int {
  int size = ANALYSIS_FFT_SIZE;
  while (size > ANALYSIS_MIN_FFT_SIZE && static_cast<std::size_t>(size) > frames) {
    size /= 2;
  }
  if (static_cast<std::size_t>(size) > frames) {
    return 0;
  }
  return size;
}

void analyse_spectrum(Analysis& analysis,
                      const float* pcm,
                      std::size_t frames,
                      std::size_t channels,
                      std::size_t active_channels,
                      int sample_rate) {
  const int fft_size = choose_fft_size(frames);
  if (fft_size == 0) {
    return;
  }
  const auto window_size = static_cast<std::size_t>(fft_size);
  const std::size_t bins = window_size / 2;
  const float bin_hz = static_cast<float>(sample_rate) / static_cast<float>(fft_size);
  const std::size_t usable = frames - window_size;
  const float scale = 1.0F / static_cast<float>(active_channels);

  std::size_t requested = ANALYSIS_MAX_SPECTRA;
  if (usable > 0) {
    requested = std::min<std::size_t>(
        ANALYSIS_MAX_SPECTRA, std::max<std::size_t>(1, usable / (window_size / 2)));
  } else {
    requested = 1;
  }
  const double stride = usable > 0 ? double(usable) / double(requested) : 0.0;

  std::vector<float> window(window_size);
  for (std::size_t i = 0; i < window_size; ++i) {
    window[i] = 0.5F - (0.5F * std::cos(2.0F * PI * static_cast<float>(i) /
                                        static_cast<float>(window_size)));
  }

  std::vector<double> average(bins, 0.0);
  std::vector<int> hot(bins, 0);
  std::vector<float> excess(bins, 0.0F);
  std::vector<std::complex<float>> spectrum(window_size);
  std::vector<float> magnitude(bins);
  std::vector<float> smoothed;
  std::vector<double> prefix;
  const std::size_t ridge_lo =
      std::min(bins, static_cast<std::size_t>(RIDGE_LO_HZ / bin_hz));
  const std::size_t ridge_hi =
      std::min(bins, static_cast<std::size_t>(RIDGE_HI_HZ / bin_hz) + 1);
  int spectra = 0;

  for (std::size_t index = 0; index < requested; ++index) {
    const auto start = static_cast<std::size_t>(double(index) * stride);
    if (start + window_size > frames) {
      break;
    }
    for (std::size_t i = 0; i < window_size; ++i) {
      float sum = 0.0F;
      const float* frame = pcm + ((start + i) * channels);
      for (std::size_t channel = 0; channel < active_channels; ++channel) {
        sum += frame[channel];
      }
      spectrum[i] = std::complex<float>(sum * scale * window[i], 0.0F);
    }
    fft_in_place(spectrum);
    double total = 0.0;
    for (std::size_t k = 0; k < bins; ++k) {
      magnitude[k] = std::abs(spectrum[k]);
      total += double(magnitude[k]);
    }
    if (total < 1e-6) {
      continue;
    }
    ++spectra;
    for (std::size_t k = 0; k < bins; ++k) {
      average[k] += double(magnitude[k]);
    }
    if (ridge_lo >= ridge_hi) {
      continue;
    }
    smooth_over_octaves(
        magnitude, prefix, smoothed, ANALYSIS_SMOOTH_OCTAVES, ridge_lo, ridge_hi);
    for (std::size_t k = ridge_lo; k < ridge_hi; ++k) {
      const float reference = std::max(smoothed[k], SILENCE);
      const float ratio = magnitude[k] / reference;
      if (ratio > RIDGE_HOT_RATIO) {
        ++hot[k];
      }
      excess[k] += 20.0F * std::log10(std::max(ratio, SILENCE));
    }
  }
  if (spectra == 0) {
    return;
  }
  analysis.spectral_frames = spectra;

  std::vector<float> mean(bins, 0.0F);
  for (std::size_t k = 0; k < bins; ++k) {
    mean[k] = static_cast<float>(average[k] / double(spectra));
  }

  const auto zone_db = [&](float lo_hz, float hi_hz) {
    const auto lo = static_cast<std::size_t>(lo_hz / bin_hz);
    const auto hi = std::min(bins, static_cast<std::size_t>(hi_hz / bin_hz) + 1);
    if (hi <= lo) {
      return -120.0F;
    }
    double energy = 0.0;
    for (std::size_t k = lo; k < hi; ++k) {
      energy += double(mean[k]) * double(mean[k]);
    }
    return to_db(static_cast<float>(std::sqrt(energy / double(hi - lo))));
  };

  analysis.body_spectrum_db = zone_db(ZONE_BODY_LO_HZ, ZONE_BODY_HI_HZ);
  analysis.presence_spectrum_db = zone_db(ZONE_PRESENCE_LO_HZ, ZONE_PRESENCE_HI_HZ);
  analysis.air_spectrum_db = zone_db(ZONE_AIR_LO_HZ, ZONE_AIR_HI_HZ);

  collect_notches(analysis,
                  mean,
                  hot,
                  excess,
                  spectra,
                  bin_hz,
                  static_cast<float>(frames) / static_cast<float>(sample_rate));
}

struct DynamicBand {
  Biquad filter;
  std::array<BiquadState, MAX_CHANNELS> state{};
  float threshold = 1.0F;
  float floor_gain = 1.0F;
  float attack = 0.0F;
  float release = 0.0F;
  float envelope = 0.0F;
  float target = 1.0F;
  float gain = 1.0F;
  std::size_t control_position = 0;

  void configure(float frequency_hz,
                 float q,
                 float threshold_db,
                 float depth_db,
                 float attack_ms,
                 float release_ms,
                 int sample_rate) {
    filter = make_bandpass(frequency_hz, q, sample_rate);
    threshold = from_db(threshold_db);
    floor_gain = from_db(-depth_db);
    const auto rate = static_cast<float>(sample_rate);
    attack = std::exp(-1.0F / std::max(1.0F, attack_ms * 0.001F * rate));
    release = std::exp(-1.0F / std::max(1.0F, release_ms * 0.001F * rate));
  }

  auto gain_for(float key) -> float {
    const float coefficient = key > envelope ? attack : release;
    envelope = key + (coefficient * (envelope - key));
    if (control_position == 0) {
      if (envelope <= threshold) {
        target = 1.0F;
      } else {
        const float over = envelope / threshold;
        target = std::max(floor_gain, std::pow(over, (1.0F / HARSH_RATIO) - 1.0F));
      }
    }
    if (++control_position >= HARSH_CONTROL_SAMPLES) {
      control_position = 0;
    }
    gain += (target - gain) * HARSH_GAIN_SMOOTHING;
    return gain;
  }
};

auto compute_limiter_gains(const std::vector<float>& block_peaks,
                           int sample_rate,
                           float ceiling,
                           float pre_gain,
                           std::vector<float>& gains) -> float {
  const std::size_t blocks = block_peaks.size();
  gains.assign(blocks + 1, 1.0F);
  if (blocks == 0) {
    return 0.0F;
  }

  std::vector<float> need(blocks, 1.0F);
  bool engaged = false;
  for (std::size_t block = 0; block < blocks; ++block) {
    const float peak = block_peaks[block] * pre_gain;
    if (peak > ceiling) {
      need[block] = ceiling / peak;
      engaged = true;
    }
  }
  if (!engaged) {
    return 0.0F;
  }

  const auto lookahead = static_cast<std::size_t>(
      std::max(1.0F,
               LIMITER_LOOKAHEAD_MS * 0.001F * static_cast<float>(sample_rate) /
                   static_cast<float>(LIMITER_BLOCK_SAMPLES)));
  const std::size_t min_radius = lookahead + 1;
  const std::size_t box_radius = std::max<std::size_t>(1, lookahead / 2);

  std::vector<float> curve(blocks, 1.0F);
  std::vector<std::size_t> window;
  window.reserve(blocks);
  std::size_t head = 0;
  std::size_t fill = 0;
  for (std::size_t block = 0; block < blocks; ++block) {
    const std::size_t limit = std::min(blocks - 1, block + min_radius);
    while (fill <= limit) {
      while (window.size() > head && need[window.back()] >= need[fill]) {
        window.pop_back();
      }
      window.push_back(fill);
      ++fill;
    }
    const std::size_t low = block > min_radius ? block - min_radius : 0;
    while (window[head] < low) {
      ++head;
    }
    curve[block] = need[window[head]];
  }

  std::vector<float> scratch(blocks, 1.0F);
  const auto box_pass = [&](const std::vector<float>& source,
                            std::vector<float>& target) {
    double sum = 0.0;
    std::size_t lo = 0;
    std::size_t hi = 0;
    for (std::size_t block = 0; block < blocks; ++block) {
      const std::size_t want_lo = block > box_radius ? block - box_radius : 0;
      const std::size_t want_hi = std::min(blocks, block + box_radius + 1);
      while (hi < want_hi) {
        sum += double(source[hi]);
        ++hi;
      }
      while (lo < want_lo) {
        sum -= double(source[lo]);
        ++lo;
      }
      target[block] = static_cast<float>(sum / double(hi - lo));
    }
  };
  box_pass(curve, scratch);
  box_pass(scratch, curve);

  const float release = std::exp(
      -static_cast<float>(LIMITER_BLOCK_SAMPLES) /
      std::max(1.0F, LIMITER_RELEASE_MS * 0.001F * static_cast<float>(sample_rate)));
  float previous = curve[0];
  float worst = 1.0F;
  for (std::size_t block = 0; block < blocks; ++block) {
    if (curve[block] > previous) {
      curve[block] = previous + ((1.0F - release) * (curve[block] - previous));
    }
    previous = curve[block];
    worst = std::min(worst, curve[block]);
    gains[block] = curve[block];
  }
  gains[blocks] = curve[blocks - 1];
  return to_db(worst);
}

void apply_limiter(float* pcm,
                   std::size_t frames,
                   std::size_t stride,
                   std::size_t channels,
                   const std::vector<float>& gains,
                   float pre_gain,
                   float ceiling) {
  if (gains.empty()) {
    return;
  }
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const std::size_t block = frame / LIMITER_BLOCK_SAMPLES;
    const float position = static_cast<float>(frame - (block * LIMITER_BLOCK_SAMPLES)) /
                           static_cast<float>(LIMITER_BLOCK_SAMPLES);
    const float gain =
        pre_gain * (gains[block] + (position * (gains[block + 1] - gains[block])));
    for (std::size_t channel = 0; channel < channels; ++channel) {
      float& sample = pcm[(frame * stride) + channel];
      sample = std::clamp(sample * gain, -ceiling, ceiling);
    }
  }
}

void scale_and_clamp(float* pcm,
                     std::size_t frames,
                     std::size_t stride,
                     std::size_t channels,
                     float gain,
                     float ceiling) {
  for (std::size_t frame = 0; frame < frames; ++frame) {
    for (std::size_t channel = 0; channel < channels; ++channel) {
      float& sample = pcm[(frame * stride) + channel];
      sample = std::clamp(sample * gain, -ceiling, ceiling);
    }
  }
}

void duplicate_channels(float* pcm,
                        std::size_t frames,
                        std::size_t stride,
                        std::size_t active) {
  if (active >= stride) {
    return;
  }
  for (std::size_t frame = 0; frame < frames; ++frame) {
    float* samples = pcm + (frame * stride);
    for (std::size_t channel = active; channel < stride; ++channel) {
      samples[channel] = samples[0];
    }
  }
}

struct ShapeContext {
  std::size_t notch_count = 0;
  std::array<Biquad, MAX_NOTCHES> notches{};
  std::array<std::array<BiquadState, MAX_NOTCHES>, MAX_CHANNELS> notch_state{};
  Biquad presence_tilt{};
  Biquad air_tilt{};
  std::array<BiquadState, MAX_CHANNELS> presence_tilt_state{};
  std::array<BiquadState, MAX_CHANNELS> air_tilt_state{};
  std::array<float, MAX_CHANNELS> dc_previous_input{};
  std::array<float, MAX_CHANNELS> dc_previous_output{};
  DynamicBand presence_band;
  DynamicBand air_band;
  LoudnessMeter loudness;
  std::vector<float> block_peaks;
  float shaped_peak = 0.0F;
  bool use_presence_tilt = false;
  bool use_air_tilt = false;
  bool use_presence_band = false;
  bool use_air_band = false;
  bool measure_loudness = false;
};

template <std::size_t FIXED_CHANNELS>
void shape_frames(float* pcm,
                  std::size_t frames,
                  std::size_t stride,
                  std::size_t runtime_channels,
                  ShapeContext& context) {
  const std::size_t CHANNELS = FIXED_CHANNELS != 0 ? FIXED_CHANNELS : runtime_channels;
  const float MONO_SCALE = 1.0F / static_cast<float>(CHANNELS);
  const std::size_t notch_count = context.notch_count;
  const bool use_presence_tilt = context.use_presence_tilt;
  const bool use_air_tilt = context.use_air_tilt;
  const bool use_presence_band = context.use_presence_band;
  const bool use_air_band = context.use_air_band;
  const bool measure_loudness = context.measure_loudness;

  float shaped_peak = context.shaped_peak;
  float block_peak = 0.0F;
  std::size_t block_position = 0;

  for (std::size_t frame = 0; frame < frames; ++frame) {
    float* samples = pcm + (frame * stride);
    float working[MAX_CHANNELS];

    for (std::size_t channel = 0; channel < CHANNELS; ++channel) {
      const float input = samples[channel];
      float sample = input - context.dc_previous_input[channel] +
                     (DC_BLOCKER_DECAY * context.dc_previous_output[channel]);
      context.dc_previous_input[channel] = input;
      context.dc_previous_output[channel] = sample;
      for (std::size_t i = 0; i < notch_count; ++i) {
        sample = context.notch_state[channel][i].process(context.notches[i], sample);
      }
      if (use_presence_tilt) {
        sample =
            context.presence_tilt_state[channel].process(context.presence_tilt, sample);
      }
      if (use_air_tilt) {
        sample = context.air_tilt_state[channel].process(context.air_tilt, sample);
      }
      working[channel] = sample;
    }

    if (use_presence_band) {
      float band[MAX_CHANNELS];
      float key = 0.0F;
      for (std::size_t channel = 0; channel < CHANNELS; ++channel) {
        band[channel] = context.presence_band.state[channel].process(
            context.presence_band.filter, working[channel]);
        key = std::max(key, std::abs(band[channel]));
      }
      const float gain = context.presence_band.gain_for(key) - 1.0F;
      for (std::size_t channel = 0; channel < CHANNELS; ++channel) {
        working[channel] += gain * band[channel];
      }
    }

    if (use_air_band) {
      float band[MAX_CHANNELS];
      float key = 0.0F;
      for (std::size_t channel = 0; channel < CHANNELS; ++channel) {
        band[channel] = context.air_band.state[channel].process(context.air_band.filter,
                                                                working[channel]);
        key = std::max(key, std::abs(band[channel]));
      }
      const float gain = context.air_band.gain_for(key) - 1.0F;
      for (std::size_t channel = 0; channel < CHANNELS; ++channel) {
        working[channel] += gain * band[channel];
      }
    }

    float mono = 0.0F;
    for (std::size_t channel = 0; channel < CHANNELS; ++channel) {
      const float shaped = working[channel];
      samples[channel] = shaped;
      const float magnitude = std::abs(shaped);
      shaped_peak = std::max(shaped_peak, magnitude);
      block_peak = std::max(block_peak, magnitude);
      mono += shaped;
    }
    if (measure_loudness) {
      context.loudness.push(mono * MONO_SCALE);
    }
    if (++block_position >= LIMITER_BLOCK_SAMPLES) {
      context.block_peaks.push_back(block_peak);
      block_peak = 0.0F;
      block_position = 0;
    }
  }
  if (block_position > 0) {
    context.block_peaks.push_back(block_peak);
  }
  context.shaped_peak = shaped_peak;
}

} // namespace

auto profile_for(Material material) -> Profile {
  Profile profile;
  switch (material) {
  case Material::Music:
    profile.normalise_loudness = true;
    profile.detect_resonances = true;
    profile.target_lufs = -15.0F;
    profile.loudness_authority_db = 6.0F;
    profile.presence_target_db = -17.0F;
    profile.air_target_db = -26.0F;
    profile.tilt_cut_db = 3.0F;
    profile.tilt_boost_db = 0.0F;
    profile.harshness_depth_db = 4.0F;
    profile.harshness_threshold_db = 8.0F;
    profile.ceiling_db = -1.0F;
    break;
  case Material::Ambience:
    profile.normalise_loudness = true;
    profile.detect_resonances = true;
    profile.target_lufs = -16.5F;
    profile.loudness_authority_db = 6.0F;
    profile.presence_target_db = -10.0F;
    profile.air_target_db = -18.0F;
    profile.tilt_cut_db = 3.0F;
    profile.tilt_boost_db = 0.0F;
    profile.harshness_depth_db = 3.5F;
    profile.harshness_threshold_db = 9.0F;
    profile.ceiling_db = -1.5F;
    break;
  case Material::Voice:
    profile.normalise_loudness = true;
    profile.detect_resonances = true;
    profile.target_lufs = -15.5F;
    profile.loudness_authority_db = VOICE_LOUDNESS_AUTHORITY_DB;
    profile.presence_target_db = -16.0F;
    profile.air_target_db = -28.0F;
    profile.tilt_cut_db = 3.0F;
    profile.tilt_boost_db = 0.0F;
    profile.harshness_depth_db = 4.5F;
    profile.harshness_threshold_db = 7.0F;
    profile.ceiling_db = -1.0F;
    break;
  case Material::Interface:
    profile.normalise_loudness = false;
    profile.makeup_db = INTERFACE_MAKEUP_DB;
    profile.presence_target_db = -8.0F;
    profile.air_target_db = -16.0F;
    profile.tilt_cut_db = 3.0F;
    profile.tilt_boost_db = 0.0F;
    profile.harshness_depth_db = 5.0F;
    profile.harshness_threshold_db = 6.0F;
    profile.ceiling_db = -1.0F;
    break;
  case Material::Effect:
    profile.normalise_loudness = false;
    profile.presence_target_db = -8.0F;
    profile.air_target_db = -16.0F;
    profile.tilt_cut_db = 2.5F;
    profile.tilt_boost_db = 0.0F;
    profile.harshness_depth_db = 4.0F;
    profile.harshness_threshold_db = 7.0F;
    profile.ceiling_db = -1.0F;
    break;
  }
  return profile;
}

auto effect_material(const std::string& resource_id) -> Material {
  std::string lowered;
  lowered.reserve(resource_id.size());
  for (const char character : resource_id) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  if (lowered.rfind("ui.", 0) == 0 || lowered.find(".ui.", 0) != std::string::npos ||
      lowered.find("/ui/") != std::string::npos) {
    return Material::Interface;
  }
  return Material::Effect;
}

auto analyse(const float* pcm,
             std::size_t frames,
             int channels,
             int sample_rate) -> Analysis {
  Analysis analysis;
  if (pcm == nullptr || frames == 0 || channels <= 0 || sample_rate <= 0) {
    return analysis;
  }
  const auto stride = static_cast<std::size_t>(channels);
  const auto active = std::min(stride, MAX_CHANNELS);
  const float scale = 1.0F / static_cast<float>(active);

  LoudnessMeter loudness;
  loudness.prepare(sample_rate);
  const Biquad presence_filter =
      make_bandpass(HARSH_PRESENCE_HZ, HARSH_PRESENCE_Q, sample_rate);
  const Biquad air_filter = make_bandpass(HARSH_AIR_HZ, HARSH_AIR_Q, sample_rate);
  BiquadState presence_state;
  BiquadState air_state;
  double presence_energy = 0.0;
  double air_energy = 0.0;
  float presence_peak = 0.0F;
  float air_peak = 0.0F;
  bool identical = active > 1;
  float peak = 0.0F;

  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float* samples = pcm + (frame * stride);
    float sum = 0.0F;
    for (std::size_t channel = 0; channel < active; ++channel) {
      const float sample = samples[channel];
      sum += sample;
      peak = std::max(peak, std::abs(sample));
      identical = identical && (sample == samples[0]);
    }
    const float mono = sum * scale;
    loudness.push(mono);
    const float presence = presence_state.process(presence_filter, mono);
    const float air = air_state.process(air_filter, mono);
    presence_energy += double(presence) * double(presence);
    air_energy += double(air) * double(air);
    presence_peak = std::max(presence_peak, std::abs(presence));
    air_peak = std::max(air_peak, std::abs(air));
  }

  analysis.peak = peak;
  analysis.channels_identical = identical;
  analysis.loudness_lufs = loudness.lufs();
  analysis.presence_band_db =
      to_db(static_cast<float>(std::sqrt(presence_energy / double(frames))));
  analysis.air_band_db =
      to_db(static_cast<float>(std::sqrt(air_energy / double(frames))));
  analysis.presence_peak_db = to_db(presence_peak);
  analysis.air_peak_db = to_db(air_peak);
  analyse_spectrum(analysis, pcm, frames, stride, active, sample_rate);
  return analysis;
}

auto apply(float* pcm,
           std::size_t frames,
           int channels,
           int sample_rate,
           const Profile& profile,
           const Analysis& analysis) -> Report {
  Report report;
  if (pcm == nullptr || frames == 0 || channels <= 0 || sample_rate <= 0) {
    return report;
  }
  const auto stride = static_cast<std::size_t>(channels);
  const auto channel_count =
      analysis.channels_identical ? std::size_t{1} : std::min(stride, MAX_CHANNELS);
  report.input_peak_db = to_db(analysis.peak);
  report.input_lufs = analysis.loudness_lufs;
  report.notch_count = profile.detect_resonances ? analysis.notch_count : 0;

  ShapeContext context;
  context.notch_count = profile.detect_resonances ? analysis.notch_count : 0;
  for (std::size_t i = 0; i < context.notch_count; ++i) {
    context.notches[i] = make_peaking(analysis.notches[i].frequency_hz,
                                      analysis.notches[i].q,
                                      analysis.notches[i].gain_db,
                                      sample_rate);
  }

  float presence_tilt = 0.0F;
  float air_tilt = 0.0F;
  if (analysis.spectral_frames > 0) {
    const float presence_relative =
        analysis.presence_spectrum_db - analysis.body_spectrum_db;
    const float air_relative = analysis.air_spectrum_db - analysis.body_spectrum_db;
    presence_tilt = std::clamp(profile.presence_target_db - presence_relative,
                               -profile.tilt_cut_db,
                               profile.tilt_boost_db);
    air_tilt = std::clamp(profile.air_target_db - air_relative,
                          -profile.tilt_cut_db,
                          profile.tilt_boost_db);
  }
  context.use_presence_tilt = std::abs(presence_tilt) > TILT_MIN_DB;
  context.use_air_tilt = std::abs(air_tilt) > TILT_MIN_DB;
  report.presence_tilt_db = context.use_presence_tilt ? presence_tilt : 0.0F;
  report.air_tilt_db = context.use_air_tilt ? air_tilt : 0.0F;
  context.presence_tilt =
      make_peaking(TILT_PRESENCE_HZ, TILT_PRESENCE_Q, presence_tilt, sample_rate);
  context.air_tilt = make_high_shelf(TILT_AIR_HZ, air_tilt, sample_rate);

  const float presence_threshold_db =
      analysis.presence_band_db + profile.harshness_threshold_db;
  const float air_threshold_db = analysis.air_band_db + profile.harshness_threshold_db +
                                 HARSH_AIR_THRESHOLD_OFFSET_DB;
  context.use_presence_band = analysis.presence_peak_db > presence_threshold_db;
  context.use_air_band = analysis.air_peak_db > air_threshold_db;
  context.presence_band.configure(HARSH_PRESENCE_HZ,
                                  HARSH_PRESENCE_Q,
                                  presence_threshold_db,
                                  profile.harshness_depth_db,
                                  HARSH_PRESENCE_ATTACK_MS,
                                  HARSH_PRESENCE_RELEASE_MS,
                                  sample_rate);
  context.air_band.configure(HARSH_AIR_HZ,
                             HARSH_AIR_Q,
                             air_threshold_db,
                             profile.harshness_depth_db,
                             HARSH_AIR_ATTACK_MS,
                             HARSH_AIR_RELEASE_MS,
                             sample_rate);

  context.measure_loudness = profile.normalise_loudness;
  if (context.measure_loudness) {
    context.loudness.prepare(sample_rate);
  }
  context.block_peaks.reserve((frames / LIMITER_BLOCK_SAMPLES) + 2);

  switch (channel_count) {
  case 1:
    shape_frames<1>(pcm, frames, stride, 1, context);
    break;
  case 2:
    shape_frames<2>(pcm, frames, stride, 2, context);
    break;
  default:
    shape_frames<0>(pcm, frames, stride, channel_count, context);
    break;
  }

  const float ceiling = from_db(profile.ceiling_db);
  float loudness_gain = from_db(profile.makeup_db);
  report.loudness_gain_db = profile.makeup_db;
  report.output_lufs = (analysis.loudness_lufs > -60.0F)
                           ? analysis.loudness_lufs + profile.makeup_db
                           : analysis.loudness_lufs;
  if (profile.normalise_loudness && analysis.loudness_lufs > -60.0F) {
    const float shaped_lufs = context.loudness.lufs();
    float gain_db = std::clamp(profile.target_lufs - shaped_lufs,
                               -profile.loudness_authority_db,
                               profile.loudness_authority_db);
    gain_db = std::min(
        gain_db, profile.ceiling_db - to_db(context.shaped_peak) + MAX_LIMITING_DB);
    report.loudness_gain_db = gain_db;
    report.output_lufs = shaped_lufs + gain_db;
    loudness_gain = from_db(gain_db);
  }

  std::vector<float> gains;
  report.limiter_reduction_db = compute_limiter_gains(
      context.block_peaks, sample_rate, ceiling, loudness_gain, gains);
  if (report.limiter_reduction_db < 0.0F) {
    apply_limiter(pcm, frames, stride, channel_count, gains, loudness_gain, ceiling);
    report.output_peak_db = profile.ceiling_db;
  } else {
    const float scaled_peak = context.shaped_peak * loudness_gain;
    if (std::abs(loudness_gain - 1.0F) > 1e-6F || scaled_peak > ceiling) {
      scale_and_clamp(pcm, frames, stride, channel_count, loudness_gain, ceiling);
    }
    report.output_peak_db = to_db(std::min(scaled_peak, ceiling));
  }
  duplicate_channels(pcm, frames, stride, channel_count);
  return report;
}

auto restore(std::vector<float>& interleaved_pcm,
             int channels,
             int sample_rate,
             Material material) -> Report {
  if (interleaved_pcm.empty() || channels <= 0) {
    return {};
  }
  const std::size_t frames =
      interleaved_pcm.size() / static_cast<std::size_t>(channels);
  const Analysis analysis =
      analyse(interleaved_pcm.data(), frames, channels, sample_rate);
  return apply(interleaved_pcm.data(),
               frames,
               channels,
               sample_rate,
               profile_for(material),
               analysis);
}

} // namespace Game::Audio::Mastering
