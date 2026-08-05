#include <algorithm>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <random>
#include <vector>

#include "game/audio/loop_seam.h"

namespace {

using Game::Audio::seal_loop;

constexpr unsigned SAMPLE_RATE = 48000;
constexpr unsigned CHANNELS = 2;

auto make_ramped_bed(std::size_t frames,
                     float ramp,
                     unsigned seed = 7U) -> std::vector<float> {
  std::vector<float> pcm(frames * CHANNELS, 0.0F);
  std::mt19937 generator(seed);
  std::uniform_real_distribution<float> noise(-0.02F, 0.02F);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float value = noise(generator) +
                        ramp * (static_cast<float>(frame) / static_cast<float>(frames));
    pcm[frame * CHANNELS] = value;
    pcm[(frame * CHANNELS) + 1] = value;
  }
  return pcm;
}

auto median_internal_step(const std::vector<float>& pcm,
                          std::size_t loop_frames) -> float {
  std::vector<float> steps;
  steps.reserve(loop_frames);
  for (std::size_t frame = 1; frame < loop_frames; ++frame) {
    steps.push_back(std::abs(pcm[frame * CHANNELS] - pcm[(frame - 1) * CHANNELS]));
  }
  std::sort(steps.begin(), steps.end());
  return steps.empty() ? 0.0F : steps[steps.size() / 2];
}

auto rms_over(const std::vector<float>& pcm,
              std::size_t from,
              std::size_t to) -> float {
  double total = 0.0;
  std::size_t count = 0;
  for (std::size_t frame = from; frame < to; ++frame) {
    const double value = pcm[frame * CHANNELS];
    total += value * value;
    ++count;
  }
  return count == 0 ? 0.0F : static_cast<float>(std::sqrt(total / double(count)));
}

TEST(LoopSeamTest, SealingRemovesTheStepTheMixerWouldSpliceIn) {
  auto pcm = make_ramped_bed(SAMPLE_RATE * 4, 0.6F);

  const auto seam = seal_loop(pcm.data(), SAMPLE_RATE * 4, CHANNELS, SAMPLE_RATE);
  ASSERT_GT(seam.loop_frames, 0U);

  EXPECT_NEAR(seam.step_before, 0.6F, 0.06F);

  const float internal = median_internal_step(pcm, seam.loop_frames);
  EXPECT_LT(seam.step_after, internal * 8.0F)
      << "wrap step " << seam.step_after << " against a median step of " << internal;
  EXPECT_LT(seam.step_after, 0.02F);
}

TEST(LoopSeamTest, TheLoopShrinksByExactlyTheFadeItFolded) {
  const std::size_t frames = SAMPLE_RATE * 4;
  auto pcm = make_ramped_bed(frames, 0.6F);

  const auto seam = seal_loop(pcm.data(), frames, CHANNELS, SAMPLE_RATE);

  EXPECT_EQ(seam.loop_frames + seam.fade_frames, frames);
  EXPECT_EQ(
      seam.fade_frames,
      static_cast<std::size_t>(Game::Audio::k_loop_fade_seconds * float(SAMPLE_RATE)));
}

TEST(LoopSeamTest, TheCrossfadeHoldsItsLevelThroughTheWrap) {
  const std::size_t frames = SAMPLE_RATE * 4;
  std::vector<float> pcm(frames * CHANNELS, 0.0F);
  std::mt19937 generator(11U);
  std::uniform_real_distribution<float> noise(-0.3F, 0.3F);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float value = noise(generator);
    pcm[frame * CHANNELS] = value;
    pcm[(frame * CHANNELS) + 1] = value;
  }
  const float before = rms_over(pcm, 0, frames);

  const auto seam = seal_loop(pcm.data(), frames, CHANNELS, SAMPLE_RATE);
  ASSERT_GT(seam.fade_frames, 0U);

  const float faded = rms_over(pcm, 0, seam.fade_frames);
  EXPECT_GT(faded, before * 0.85F)
      << "the crossfade dipped: " << faded << " against " << before;
  EXPECT_LT(faded, before * 1.15F);
}

TEST(LoopSeamTest, ContentOutsideTheFadeIsUntouched) {
  const std::size_t frames = SAMPLE_RATE * 4;
  auto pcm = make_ramped_bed(frames, 0.6F);
  const std::vector<float> original = pcm;

  const auto seam = seal_loop(pcm.data(), frames, CHANNELS, SAMPLE_RATE);
  ASSERT_GT(seam.fade_frames, 0U);

  for (std::size_t frame = seam.fade_frames; frame < seam.loop_frames; ++frame) {
    ASSERT_FLOAT_EQ(pcm[frame * CHANNELS], original[frame * CHANNELS]) << frame;
  }
}

TEST(LoopSeamTest, ABedTooShortToFoldIsLeftAlone) {
  std::vector<float> pcm(8 * CHANNELS, 0.5F);
  const std::vector<float> original = pcm;

  const auto seam = seal_loop(pcm.data(), 8, CHANNELS, SAMPLE_RATE);

  EXPECT_EQ(seam.loop_frames, 0U);
  EXPECT_EQ(pcm, original);
}

TEST(LoopSeamTest, TheFadeNeverEatsMostOfAShortBed) {
  const std::size_t frames = SAMPLE_RATE / 2;
  auto pcm = make_ramped_bed(frames, 0.6F);

  const auto seam = seal_loop(pcm.data(), frames, CHANNELS, SAMPLE_RATE);

  ASSERT_GT(seam.loop_frames, 0U);
  EXPECT_LE(static_cast<float>(seam.fade_frames),
            Game::Audio::k_max_loop_fade_fraction * static_cast<float>(frames) + 1.0F);
  EXPECT_GT(seam.loop_frames, frames * 8 / 10);
}

} // namespace
