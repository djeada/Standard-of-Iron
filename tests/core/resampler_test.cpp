#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "game/audio/resampler.h"

namespace {

using Game::Audio::resample_to;

constexpr unsigned CHANNELS = 2;

auto make_tone(unsigned rate,
               double seconds,
               double hz,
               float amplitude = 0.5F) -> std::vector<float> {
  const auto frames = static_cast<std::size_t>(rate * seconds);
  std::vector<float> pcm(frames * CHANNELS, 0.0F);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const double phase =
        2.0 * std::numbers::pi * hz * static_cast<double>(frame) / rate;
    const auto value = static_cast<float>(amplitude * std::sin(phase));
    pcm[frame * CHANNELS] = value;
    pcm[(frame * CHANNELS) + 1] = value;
  }
  return pcm;
}

auto make_band_limited_noise(unsigned rate, double seconds) -> std::vector<float> {
  const auto frames = static_cast<std::size_t>(rate * seconds);
  std::vector<float> pcm(frames * CHANNELS, 0.0F);
  std::uint32_t state = 22695477U;
  float previous = 0.0F;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    state = (state * 1103515245U) + 12345U;
    const float white = ((static_cast<float>(state >> 16U) / 32768.0F) - 1.0F) * 0.3F;
    previous = (previous * 0.5F) + (white * 0.5F);
    pcm[frame * CHANNELS] = previous;
    pcm[(frame * CHANNELS) + 1] = previous;
  }
  return pcm;
}

auto linear_upsample(const std::vector<float>& pcm,
                     unsigned rate_in,
                     unsigned rate_out) -> std::vector<float> {
  const std::size_t frames_in = pcm.size() / CHANNELS;
  const auto frames_out =
      static_cast<std::size_t>(frames_in * (double(rate_out) / rate_in));
  std::vector<float> out(frames_out * CHANNELS, 0.0F);
  for (std::size_t frame = 0; frame < frames_out; ++frame) {
    const double source = frame * (double(rate_in) / rate_out);
    const auto index = static_cast<std::size_t>(source);
    const auto fraction = static_cast<float>(source - double(index));
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      const float a = pcm[std::min(index, frames_in - 1) * CHANNELS + channel];
      const float b = pcm[std::min(index + 1, frames_in - 1) * CHANNELS + channel];
      out[frame * CHANNELS + channel] = a + (b - a) * fraction;
    }
  }
  return out;
}

auto tone_level_db(const std::vector<float>& pcm, unsigned rate, double hz) -> float {
  const std::size_t frames = pcm.size() / CHANNELS;
  const std::size_t skip = std::min<std::size_t>(frames / 4, rate / 10);
  const double omega = 2.0 * std::numbers::pi * hz / rate;
  const double coefficient = 2.0 * std::cos(omega);
  double s1 = 0.0;
  double s2 = 0.0;
  std::size_t counted = 0;
  for (std::size_t frame = skip; frame + skip < frames; ++frame) {
    const double s0 = pcm[frame * CHANNELS] + coefficient * s1 - s2;
    s2 = s1;
    s1 = s0;
    ++counted;
  }
  if (counted == 0) {
    return -999.0F;
  }
  const double power = (s1 * s1) + (s2 * s2) - (coefficient * s1 * s2);
  const double magnitude = std::sqrt(std::max(0.0, power)) * 2.0 / double(counted);
  return static_cast<float>(20.0 * std::log10(magnitude + 1.0e-12));
}

auto band_energy_db(const std::vector<float>& pcm,
                    unsigned rate,
                    double from_hz,
                    double to_hz) -> float {
  double total = 0.0;
  int bins = 0;
  for (double hz = from_hz; hz <= to_hz; hz += 100.0) {
    const float level = tone_level_db(pcm, rate, hz);
    total += std::pow(10.0, level / 10.0);
    ++bins;
  }
  return bins == 0 ? -999.0F
                   : static_cast<float>(10.0 * std::log10(total / bins + 1.0e-20));
}

TEST(ResamplerTest, RatesThatAlreadyMatchAreLeftAlone) {
  auto pcm = make_tone(48000, 0.2, 1000.0);
  const std::vector<float> original = pcm;

  const auto report = resample_to(pcm, CHANNELS, 48000, 48000);

  EXPECT_FALSE(report.applied);
  EXPECT_EQ(pcm, original);
}

TEST(ResamplerTest, UpsamplingKeepsTheToneAtItsOwnFrequencyAndLevel) {
  auto pcm = make_tone(16000, 0.5, 1000.0, 0.5F);

  const auto report = resample_to(pcm, CHANNELS, 16000, 48000);

  ASSERT_TRUE(report.applied);
  EXPECT_EQ(report.up, 3U);
  EXPECT_EQ(report.down, 1U);
  EXPECT_EQ(report.frames_out, static_cast<std::size_t>(0.5 * 48000));

  const float level = tone_level_db(pcm, 48000, 1000.0);
  EXPECT_NEAR(level, 20.0F * std::log10(0.5F), 0.5F)
      << "the resampler changed the level of a plain tone";
}

TEST(ResamplerTest, UpsamplingDoesNotFabricateContentAboveTheSourceNyquist) {
  const std::vector<float> source = make_band_limited_noise(16000, 1.0);
  std::vector<float> pcm = source;

  const auto report = resample_to(pcm, CHANNELS, 16000, 48000);
  ASSERT_TRUE(report.applied);

  const float passband = band_energy_db(pcm, 48000, 1000.0, 4000.0);
  const float images = band_energy_db(pcm, 48000, 9000.0, 15000.0);
  EXPECT_LT(images, passband - 50.0F)
      << "imaging at " << images << " dB against a passband of " << passband << " dB";

  const std::vector<float> naive = linear_upsample(source, 16000, 48000);
  const float naive_images = band_energy_db(naive, 48000, 9000.0, 15000.0);
  EXPECT_LT(images, naive_images - 25.0F)
      << "no better than interpolating: " << images << " dB against " << naive_images;
}

TEST(ResamplerTest, ASingleToneNearNyquistDoesNotMirrorItself) {
  auto pcm = make_tone(16000, 0.5, 6000.0, 0.5F);

  const auto report = resample_to(pcm, CHANNELS, 16000, 48000);
  ASSERT_TRUE(report.applied);

  const float wanted = tone_level_db(pcm, 48000, 6000.0);

  const float mirrored = tone_level_db(pcm, 48000, 10000.0);

  EXPECT_GT(wanted, -8.0F);
  EXPECT_LT(mirrored, wanted - 60.0F)
      << "mirror image at " << mirrored << " dB against the tone at " << wanted;
}

TEST(ResamplerTest, TheAudibleBandSurvivesTheAntiImagingFilter) {
  auto pcm = make_tone(16000, 0.5, 7000.0, 0.5F);

  ASSERT_TRUE(resample_to(pcm, CHANNELS, 16000, 48000).applied);

  const float level = tone_level_db(pcm, 48000, 7000.0);
  EXPECT_NEAR(level, 20.0F * std::log10(0.5F), 1.5F)
      << "the filter ate real content just below the source Nyquist";
}

TEST(ResamplerTest, FractionalRatiosResampleToTheRightLength) {
  auto pcm = make_tone(32000, 0.5, 1000.0);

  const auto report = resample_to(pcm, CHANNELS, 32000, 48000);

  ASSERT_TRUE(report.applied);
  EXPECT_EQ(report.up, 3U);
  EXPECT_EQ(report.down, 2U);
  EXPECT_EQ(report.frames_out, static_cast<std::size_t>(0.5 * 48000));
  EXPECT_NEAR(tone_level_db(pcm, 48000, 1000.0), 20.0F * std::log10(0.5F), 0.5F);
}

TEST(ResamplerTest, MonoContentStaysIdenticalAcrossChannels) {
  auto pcm = make_band_limited_noise(16000, 0.3);

  ASSERT_TRUE(resample_to(pcm, CHANNELS, 16000, 48000).applied);

  for (std::size_t frame = 0; frame < pcm.size() / CHANNELS; ++frame) {
    ASSERT_FLOAT_EQ(pcm[frame * CHANNELS], pcm[(frame * CHANNELS) + 1]) << frame;
  }
}

TEST(ResamplerTest, EmptyOrDegenerateInputIsRefusedRatherThanCrashing) {
  std::vector<float> empty;
  EXPECT_FALSE(resample_to(empty, CHANNELS, 16000, 48000).applied);

  auto pcm = make_tone(16000, 0.1, 500.0);
  EXPECT_FALSE(resample_to(pcm, 0, 16000, 48000).applied);
  EXPECT_FALSE(resample_to(pcm, CHANNELS, 0, 48000).applied);
  EXPECT_FALSE(resample_to(pcm, CHANNELS, 16000, 0).applied);
}

} // namespace
