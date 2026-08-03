#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <vector>

#include "game/audio/audio_mastering.h"
#include "game/audio/bus_limiter.h"

namespace {

using Game::Audio::BusLimiter;
namespace Mastering = Game::Audio::Mastering;

constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;
constexpr float PI = 3.14159265358979323846F;

auto make_stereo(std::size_t frames) -> std::vector<float> {
  return std::vector<float>(frames * CHANNELS, 0.0F);
}

void add_tone(std::vector<float>& pcm, float frequency_hz, float amplitude) {
  const std::size_t frames = pcm.size() / CHANNELS;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float phase = 2.0F * PI * frequency_hz * static_cast<float>(frame) /
                        static_cast<float>(SAMPLE_RATE);
    const float value = amplitude * std::sin(phase);
    pcm[(frame * CHANNELS)] += value;
    pcm[(frame * CHANNELS) + 1] += value;
  }
}

void add_noise(std::vector<float>& pcm, float amplitude, unsigned seed) {
  std::mt19937 generator(seed);
  std::uniform_real_distribution<float> distribution(-amplitude, amplitude);
  const std::size_t frames = pcm.size() / CHANNELS;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float value = distribution(generator);
    pcm[(frame * CHANNELS)] += value;
    pcm[(frame * CHANNELS) + 1] += value;
  }
}

auto peak_of(const std::vector<float>& pcm) -> float {
  float peak = 0.0F;
  for (const float sample : pcm) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

auto rms_of(const std::vector<float>& pcm) -> float {
  if (pcm.empty()) {
    return 0.0F;
  }
  double energy = 0.0;
  for (const float sample : pcm) {
    energy += double(sample) * double(sample);
  }
  return static_cast<float>(std::sqrt(energy / double(pcm.size())));
}

auto tone_magnitude(const std::vector<float>& pcm, float frequency_hz) -> float {
  const std::size_t frames = pcm.size() / CHANNELS;
  double real = 0.0;
  double imaginary = 0.0;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const double phase =
        2.0 * double(PI) * double(frequency_hz) * double(frame) / double(SAMPLE_RATE);
    const double value = pcm[frame * CHANNELS];
    real += value * std::cos(phase);
    imaginary += value * std::sin(phase);
  }
  return static_cast<float>(std::sqrt((real * real) + (imaginary * imaginary)) /
                            double(frames));
}

auto master(std::vector<float>& pcm,
            Mastering::Material material) -> Mastering::Report {
  return Mastering::restore(pcm, CHANNELS, SAMPLE_RATE, material);
}

TEST(AudioMastering, LoudMusicNeverLeavesTheCeiling) {
  auto pcm = make_stereo(SAMPLE_RATE * 4);
  add_tone(pcm, 220.0F, 0.9F);
  add_tone(pcm, 660.0F, 0.5F);
  add_noise(pcm, 0.2F, 1234);
  ASSERT_GT(peak_of(pcm), 1.0F);

  const Mastering::Report report = master(pcm, Mastering::Material::Music);

  const float ceiling = std::pow(10.0F, -1.0F / 20.0F);
  EXPECT_LE(peak_of(pcm), ceiling + 1e-4F);
  EXPECT_LT(report.output_peak_db, -0.9F);
  EXPECT_GT(report.input_peak_db, 0.0F);
}

TEST(AudioMastering, EveryMaterialRespectsItsOwnCeiling) {
  const Mastering::Material materials[] = {
      Mastering::Material::Music,
      Mastering::Material::Ambience,
      Mastering::Material::Voice,
      Mastering::Material::Interface,
      Mastering::Material::Effect,
  };
  for (const Mastering::Material material : materials) {
    auto pcm = make_stereo(SAMPLE_RATE * 2);
    add_tone(pcm, 440.0F, 1.4F);
    master(pcm, material);
    const float ceiling =
        std::pow(10.0F, Mastering::profile_for(material).ceiling_db / 20.0F);
    EXPECT_LE(peak_of(pcm), ceiling + 1e-4F);
  }
}

TEST(AudioMastering, MusicLandsOnTheLoudnessTarget) {
  const Mastering::Profile profile = Mastering::profile_for(Mastering::Material::Music);

  auto quiet = make_stereo(SAMPLE_RATE * 5);
  add_noise(quiet, 0.16F, 99);
  const Mastering::Report quiet_report = master(quiet, Mastering::Material::Music);

  auto loud = make_stereo(SAMPLE_RATE * 5);
  add_noise(loud, 0.40F, 99);
  const Mastering::Report loud_report = master(loud, Mastering::Material::Music);

  EXPECT_GT(quiet_report.loudness_gain_db, 0.0F);
  EXPECT_LT(loud_report.loudness_gain_db, 0.0F);
  EXPECT_NEAR(quiet_report.output_lufs, profile.target_lufs, 1.0F);
  EXPECT_NEAR(loud_report.output_lufs, profile.target_lufs, 1.0F);
}

TEST(AudioMastering, LoudnessCorrectionStaysWithinItsAuthority) {
  const Mastering::Profile profile = Mastering::profile_for(Mastering::Material::Music);

  auto very_quiet = make_stereo(SAMPLE_RATE * 3);
  add_noise(very_quiet, 0.004F, 11);
  const Mastering::Report report = master(very_quiet, Mastering::Material::Music);

  EXPECT_FLOAT_EQ(report.loudness_gain_db, profile.loudness_authority_db);
  EXPECT_LT(report.output_lufs, profile.target_lufs);
}

TEST(AudioMastering, StationaryResonanceIsFoundAndReduced) {
  auto pcm = make_stereo(SAMPLE_RATE * 24);
  add_noise(pcm, 0.30F, 7);
  add_tone(pcm, 6000.0F, 0.06F);

  const Mastering::Analysis analysis =
      Mastering::analyse(pcm.data(), pcm.size() / CHANNELS, CHANNELS, SAMPLE_RATE);
  ASSERT_GE(analysis.notch_count, 1U);

  bool found = false;
  for (std::size_t i = 0; i < analysis.notch_count; ++i) {
    if (std::abs(analysis.notches[i].frequency_hz - 6000.0F) < 250.0F) {
      found = true;
      EXPECT_LT(analysis.notches[i].gain_db, -1.0F);
    }
  }
  EXPECT_TRUE(found);

  const float before = tone_magnitude(pcm, 6000.0F) / tone_magnitude(pcm, 1000.0F);
  master(pcm, Mastering::Material::Music);
  const float after = tone_magnitude(pcm, 6000.0F) / tone_magnitude(pcm, 1000.0F);
  EXPECT_LT(after, before * 0.85F);
}

TEST(AudioMastering, BroadbandContentIsLeftAlone) {
  auto pcm = make_stereo(SAMPLE_RATE * 24);
  add_noise(pcm, 0.2F, 4242);

  const Mastering::Analysis analysis =
      Mastering::analyse(pcm.data(), pcm.size() / CHANNELS, CHANNELS, SAMPLE_RATE);
  EXPECT_EQ(analysis.notch_count, 0U);
}

TEST(AudioMastering, AuthoredCuesKeepTheirResonances) {
  auto source = make_stereo(SAMPLE_RATE * 24);
  add_noise(source, 0.30F, 7);
  add_tone(source, 6000.0F, 0.06F);

  const Mastering::Analysis analysis = Mastering::analyse(
      source.data(), source.size() / CHANNELS, CHANNELS, SAMPLE_RATE);
  ASSERT_GE(analysis.notch_count, 1U);
  const float before =
      tone_magnitude(source, 6000.0F) / tone_magnitude(source, 1000.0F);

  float authored_ratio = 0.0F;
  for (const Mastering::Material material :
       {Mastering::Material::Effect, Mastering::Material::Interface}) {
    EXPECT_FALSE(Mastering::profile_for(material).detect_resonances);
    auto pcm = source;
    const Mastering::Report report = master(pcm, material);
    EXPECT_EQ(report.notch_count, 0U);
    authored_ratio = tone_magnitude(pcm, 6000.0F) / tone_magnitude(pcm, 1000.0F);
  }

  auto music = source;
  const Mastering::Report music_report = master(music, Mastering::Material::Music);
  EXPECT_GT(music_report.notch_count, 0U);
  const float music_ratio =
      tone_magnitude(music, 6000.0F) / tone_magnitude(music, 1000.0F);
  EXPECT_LT(music_ratio, before * 0.85F);
  EXPECT_LT(music_ratio, authored_ratio * 0.9F);
}

TEST(AudioMastering, BriefMaterialIsNeverNotched) {
  auto brief = make_stereo(SAMPLE_RATE * 5);
  add_noise(brief, 0.30F, 7);
  add_tone(brief, 6000.0F, 0.06F);
  const Mastering::Analysis brief_analysis =
      Mastering::analyse(brief.data(), brief.size() / CHANNELS, CHANNELS, SAMPLE_RATE);
  EXPECT_EQ(brief_analysis.notch_count, 0U);
}

TEST(AudioMastering, ShortClipsAreNeverNotched) {
  auto pcm = make_stereo(4096);
  add_tone(pcm, 3000.0F, 0.4F);
  add_noise(pcm, 0.05F, 5);

  const Mastering::Analysis analysis =
      Mastering::analyse(pcm.data(), pcm.size() / CHANNELS, CHANNELS, SAMPLE_RATE);
  EXPECT_EQ(analysis.notch_count, 0U);
  EXPECT_LT(analysis.spectral_frames, 24);
}

TEST(AudioMastering, AuthoredEffectLevelsSurvive) {
  auto pcm = make_stereo(8192);
  add_tone(pcm, 900.0F, 0.18F);
  const float before = peak_of(pcm);

  const Mastering::Report report = master(pcm, Mastering::Material::Effect);

  EXPECT_NEAR(peak_of(pcm), before, 0.02F);
  EXPECT_FLOAT_EQ(report.loudness_gain_db, 0.0F);
  EXPECT_EQ(report.notch_count, 0U);
}

TEST(AudioMastering, SilenceStaysSilent) {
  auto pcm = make_stereo(SAMPLE_RATE);
  master(pcm, Mastering::Material::Music);
  EXPECT_EQ(peak_of(pcm), 0.0F);
}

TEST(AudioMastering, DegenerateInputsAreSafe) {
  std::vector<float> empty;
  EXPECT_NO_THROW(
      Mastering::restore(empty, CHANNELS, SAMPLE_RATE, Mastering::Material::Music));

  auto pcm = make_stereo(128);
  EXPECT_NO_THROW(Mastering::restore(pcm, 0, SAMPLE_RATE, Mastering::Material::Music));
  EXPECT_NO_THROW(Mastering::restore(pcm, CHANNELS, 0, Mastering::Material::Music));

  const Mastering::Analysis analysis =
      Mastering::analyse(nullptr, 100, CHANNELS, SAMPLE_RATE);
  EXPECT_EQ(analysis.notch_count, 0U);
}

TEST(AudioMastering, InterfaceCuesAreRecognisedByIdPrefix) {
  EXPECT_EQ(Mastering::effect_material("ui.click"), Mastering::Material::Interface);
  EXPECT_EQ(Mastering::effect_material("UI.Hover"), Mastering::Material::Interface);
  EXPECT_EQ(Mastering::effect_material("combat.sword_hit"),
            Mastering::Material::Effect);
  EXPECT_EQ(Mastering::effect_material("build.construction_started"),
            Mastering::Material::Effect);
}

TEST(BusLimiterTest, KeepsASummedMixUnderTheCeiling) {
  BusLimiter limiter;
  limiter.prepare(SAMPLE_RATE, CHANNELS);
  ASSERT_TRUE(limiter.is_ready());

  auto pcm = make_stereo(SAMPLE_RATE);
  add_tone(pcm, 200.0F, 1.2F);
  add_tone(pcm, 1500.0F, 0.8F);
  limiter.process(pcm.data(), SAMPLE_RATE);

  EXPECT_LE(peak_of(pcm), BusLimiter::DEFAULT_CEILING + 1e-4F);
  EXPECT_LT(limiter.gain(), 1.0F);
}

TEST(BusLimiterTest, LeavesQuietMixesUntouched) {
  BusLimiter limiter;
  limiter.prepare(SAMPLE_RATE, CHANNELS);

  auto pcm = make_stereo(SAMPLE_RATE);
  add_tone(pcm, 440.0F, 0.25F);
  auto expected = pcm;
  limiter.process(pcm.data(), SAMPLE_RATE);

  const std::size_t lookahead = static_cast<std::size_t>(
      BusLimiter::DEFAULT_LOOKAHEAD_MS * 0.001F * static_cast<float>(SAMPLE_RATE));
  for (std::size_t frame = lookahead + 1; frame < SAMPLE_RATE; ++frame) {
    EXPECT_NEAR(pcm[frame * CHANNELS], expected[(frame - lookahead) * CHANNELS], 1e-5F);
  }
  EXPECT_FLOAT_EQ(limiter.gain(), 1.0F);
}

TEST(BusLimiterTest, RecoversAfterATransient) {
  BusLimiter limiter;
  limiter.prepare(SAMPLE_RATE, CHANNELS);

  auto burst = make_stereo(SAMPLE_RATE / 10);
  add_tone(burst, 300.0F, 1.6F);
  limiter.process(burst.data(), static_cast<unsigned>(burst.size() / CHANNELS));
  const float reduced = limiter.gain();
  ASSERT_LT(reduced, 0.9F);

  auto quiet = make_stereo(SAMPLE_RATE);
  add_tone(quiet, 300.0F, 0.1F);
  limiter.process(quiet.data(), static_cast<unsigned>(quiet.size() / CHANNELS));
  EXPECT_GT(limiter.gain(), 0.99F);
  EXPECT_LE(peak_of(quiet), BusLimiter::DEFAULT_CEILING + 1e-4F);
}

} // namespace
