#include "audio_spectrum_analysis.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

#include "audio_dsp_math.h"

namespace Game::Audio::Mastering {

namespace {

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

} // namespace

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

} // namespace Game::Audio::Mastering
