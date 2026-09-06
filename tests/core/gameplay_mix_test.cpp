#include <array>
#include <cmath>
#include <gtest/gtest.h>

#include "game/audio/bus_limiter.h"
#include "game/audio/gameplay_mix.h"
#include "game/audio/spatial.h"

namespace {
using namespace Game::Audio;

auto settle(GameplayMix& mix, int samples = 48000) -> MixGains {
  MixGains result{};
  for (int i = 0; i < samples; ++i) {
    result = mix.next();
  }
  return result;
}

TEST(GameplayMix, RoutesSemanticCuesAndDirectResources) {
  EXPECT_EQ(mix_bus_for("alert.commander_message", MixBus::Combat), MixBus::Alert);
  EXPECT_EQ(mix_bus_for("sound_sfx.economy.income_tick", MixBus::Combat),
            MixBus::Economy);
  EXPECT_EQ(mix_bus_for("ambient.weather_rain", MixBus::Ambience), MixBus::Weather);
  EXPECT_EQ(mix_bus_for("sfx.wildlife.wolf_bite", MixBus::Combat), MixBus::Environment);
  EXPECT_EQ(mix_bus_for("sfx.movement.footstep", MixBus::Unmixed), MixBus::Combat);
  EXPECT_EQ(mix_bus_for("unknown", MixBus::Voice), MixBus::Voice);
}

TEST(GameplayMix, DuckingIsBoundedAndRecoversSmoothly) {
  GameplayMix mix;
  mix.prepare(48000);
  mix.target({}, true, true, ListeningPreset::Speakers);
  const auto first = mix.next();
  EXPECT_NEAR(first[mix_index(MixBus::Music)], 0.50F, 0.001F);
  const auto ducked = settle(mix);
  EXPECT_NEAR(ducked[mix_index(MixBus::Music)], 0.50F * 0.63F, 0.0001F);
  EXPECT_NEAR(ducked[mix_index(MixBus::Combat)], 0.40F * 0.63F, 0.0001F);
  EXPECT_FLOAT_EQ(ducked[mix_index(MixBus::Voice)], 0.89F);
  EXPECT_FLOAT_EQ(ducked[mix_index(MixBus::Alert)], 0.79F);
  mix.target({}, false, false, ListeningPreset::Speakers);
  EXPECT_NEAR(
      mix.next()[mix_index(MixBus::Music)], ducked[mix_index(MixBus::Music)], 0.001F);
  EXPECT_NEAR(settle(mix, 96000)[mix_index(MixBus::Music)], 0.50F, 0.001F);
}

TEST(GameplayMix, RoutineVoicesDoNotTriggerCriticalDepth) {
  GameplayMix mix;
  mix.prepare(48000);
  mix.target({}, true, false, ListeningPreset::Speakers);
  EXPECT_NEAR(settle(mix)[mix_index(MixBus::Music)], 0.50F * 0.85F, 0.0001F);
}

TEST(GameplayMix, DensityBudgetSpansCombatEconomyAndWildlife) {
  GameplayMix mix;
  mix.prepare(48000);
  MixCounts counts{};
  counts[mix_index(MixBus::Combat)] = 8;
  counts[mix_index(MixBus::Economy)] = 2;
  counts[mix_index(MixBus::Environment)] = 2;
  mix.target(counts, false, false, ListeningPreset::Speakers);
  const auto gains = settle(mix);
  EXPECT_NEAR(gains[mix_index(MixBus::Combat)], 0.20F, 0.0001F);
  EXPECT_NEAR(gains[mix_index(MixBus::Economy)], 0.16F, 0.0001F);
  EXPECT_FLOAT_EQ(gains[mix_index(MixBus::Voice)], 0.89F);
}

TEST(GameplayMix, SampleRateDoesNotChangeEnvelopeTiming) {
  GameplayMix slow;
  GameplayMix fast;
  slow.prepare(24000);
  fast.prepare(48000);
  slow.target({}, true, true, ListeningPreset::Night);
  fast.target({}, true, true, ListeningPreset::Night);
  EXPECT_NEAR(settle(slow, 480)[mix_index(MixBus::Music)],
              settle(fast, 960)[mix_index(MixBus::Music)],
              0.0001F);
}

TEST(GameplayMix, PresetsReduceLoudPassagesAndKeepStereoLinked) {
  std::array<float, 3> peaks{};
  for (int preset = 0; preset < 3; ++preset) {
    BusLimiter limiter;
    limiter.prepare(48000, 2);
    limiter.set_listening_preset(preset_from_int(preset));
    for (int frame = 0; frame < 48000; ++frame) {
      float pcm[2] = {0.8F, 0.4F};
      limiter.process(pcm, 1);
      EXPECT_LE(std::abs(pcm[0]), BusLimiter::GAMEPLAY_CEILING);
      EXPECT_NEAR(pcm[0], pcm[1] * 2.0F, 0.00001F);
      if (frame > 24000) {
        peaks[preset] = std::max(peaks[preset], pcm[0]);
      }
    }
  }
  EXPECT_GT(peaks[0], peaks[1]);
  EXPECT_GT(peaks[1], peaks[2]);
}

TEST(GameplayMix, CorrelatedImpactStormCannotClipAnyPreset) {
  for (int preset = 0; preset < 3; ++preset) {
    BusLimiter limiter;
    limiter.prepare(48000, 2);
    limiter.set_listening_preset(preset_from_int(preset));
    for (int frame = 0; frame < 48000; ++frame) {
      const float storm = frame % 4000 < 2000 ? 64.0F : -64.0F;
      float pcm[2] = {storm, storm};
      limiter.process(pcm, 1);
      ASSERT_TRUE(std::isfinite(pcm[0]));
      ASSERT_LE(std::abs(pcm[0]), BusLimiter::GAMEPLAY_CEILING);
      ASSERT_FLOAT_EQ(pcm[0], pcm[1]);
    }
  }
}

TEST(GameplayMix, SpatialRolloffWorksAtRtsAndCommanderHeights) {
  for (float height : {2.0F, 40.0F, 60.0F}) {
    AudioListener listener{.position = {0.0F, height, 0.0F}, .valid = true};
    const auto near = spatialize(listener, {0.0F, 0.0F, 0.0F});
    const auto middle = spatialize(listener, {0.0F, 0.0F, 45.0F});
    const auto far = spatialize(listener, {0.0F, 0.0F, 100.0F});
    EXPECT_GT(near.volume_scale, middle.volume_scale);
    EXPECT_GT(near.volume_scale, 0.1F);
    EXPECT_FLOAT_EQ(far.volume_scale, 0.0F);
    EXPECT_FLOAT_EQ(near.pan, 0.0F);
    EXPECT_NEAR(spatialize(listener, {26.0F, 0.0F, 0.0F}).pan,
                -spatialize(listener, {-26.0F, 0.0F, 0.0F}).pan,
                0.00001F);
  }
}
} // namespace
