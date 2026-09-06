#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "game/audio/miniaudio_backend.h"

namespace {

namespace Mastering = Game::Audio::Mastering;

constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;
constexpr int TONE_FRAMES = SAMPLE_RATE / 4;

auto write_tone_wav(const QString& path, float amplitude) -> bool {
  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);

  const std::uint32_t payload = TONE_FRAMES * CHANNELS * 2;
  stream.writeRawData("RIFF", 4);
  stream << std::uint32_t(36 + payload);
  stream.writeRawData("WAVEfmt ", 8);
  stream << std::uint32_t(16) << std::uint16_t(1) << std::uint16_t(CHANNELS)
         << std::uint32_t(SAMPLE_RATE) << std::uint32_t(SAMPLE_RATE * CHANNELS * 2)
         << std::uint16_t(CHANNELS * 2) << std::uint16_t(16);
  stream.writeRawData("data", 4);
  stream << payload;
  for (int frame = 0; frame < TONE_FRAMES; ++frame) {
    const double phase = 2.0 * std::numbers::pi * 440.0 * frame / SAMPLE_RATE;
    const auto value = static_cast<std::int16_t>(amplitude * std::sin(phase) * 32000.0);
    stream << value << value;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(data) == data.size();
}

auto write_unclosed_bed_wav(const QString& path, int frames, float amplitude) -> bool {
  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);

  const std::uint32_t payload = std::uint32_t(frames) * CHANNELS * 2;
  stream.writeRawData("RIFF", 4);
  stream << std::uint32_t(36 + payload);
  stream.writeRawData("WAVEfmt ", 8);
  stream << std::uint32_t(16) << std::uint16_t(1) << std::uint16_t(CHANNELS)
         << std::uint32_t(SAMPLE_RATE) << std::uint32_t(SAMPLE_RATE * CHANNELS * 2)
         << std::uint16_t(CHANNELS * 2) << std::uint16_t(16);
  stream.writeRawData("data", 4);
  stream << payload;
  const double cycles = 200.25;
  for (int frame = 0; frame < frames; ++frame) {
    const double phase = 2.0 * std::numbers::pi * cycles * frame / frames;
    const auto value = static_cast<std::int16_t>(amplitude * std::sin(phase) * 32000.0);
    stream << value << value;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  return file.write(data) == data.size();
}

auto peak_of(const std::vector<float>& buffer) -> float {
  float peak = 0.0F;
  for (const float sample : buffer) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

class AudioBackendTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(m_directory.isValid());
    m_path = m_directory.filePath("tone.wav");
    ASSERT_TRUE(write_tone_wav(m_path, 0.5F));
    ASSERT_TRUE(m_backend.initialize(
        SAMPLE_RATE, CHANNELS, MiniaudioBackend::DEFAULT_MUSIC_CHANNELS, false));
  }

  void TearDown() override { m_backend.shutdown(); }

  auto render(unsigned frames) -> std::vector<float> {
    std::vector<float> buffer(static_cast<std::size_t>(frames) * CHANNELS, 0.0F);
    m_backend.on_audio(buffer.data(), frames);
    return buffer;
  }

  QTemporaryDir m_directory;
  QString m_path;
  MiniaudioBackend m_backend;
};

TEST_F(AudioBackendTest, RequestedTracksDecodeOffTheCallingThread) {
  EXPECT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();
  EXPECT_TRUE(m_backend.is_track_ready(QStringLiteral("tone")));
}

TEST_F(AudioBackendTest, PlayingASoundReachesTheMix) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  EXPECT_LT(peak_of(render(256)), 1e-6F);

  m_backend.play_sound(QStringLiteral("tone"), 1.0F, false);
  EXPECT_GT(peak_of(render(256)), 0.01F);
  EXPECT_TRUE(m_backend.is_sound_active(QStringLiteral("tone")));

  m_backend.stop_sound(QStringLiteral("tone"));
  render(256);
  EXPECT_LT(peak_of(render(256)), 1e-6F);
  EXPECT_FALSE(m_backend.is_sound_active(QStringLiteral("tone")));
}

TEST_F(AudioBackendTest, MusicStartsOnceItsDecodeLands) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("bed"), m_path, Mastering::Material::Music));

  m_backend.play(0, QStringLiteral("bed"), 1.0F, true, 0);
  m_backend.wait_for_decodes();

  EXPECT_GT(peak_of(render(1024)), 0.01F);
  EXPECT_TRUE(m_backend.channel_playing(0));
  EXPECT_TRUE(m_backend.any_channel_playing());

  m_backend.stop(0, 0);
  render(1024);
  EXPECT_FALSE(m_backend.any_channel_playing());
}

TEST_F(AudioBackendTest, UnloadingAReferencedTrackIsSafe) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();
  m_backend.play_sound(QStringLiteral("tone"), 1.0F, true);
  render(256);

  m_backend.unload(QStringLiteral("tone"));

  EXPECT_FALSE(m_backend.is_track_ready(QStringLiteral("tone")));
  EXPECT_FALSE(m_backend.is_sound_active(QStringLiteral("tone")));
  render(256);
  EXPECT_LT(peak_of(render(256)), 1e-6F);
}

TEST_F(AudioBackendTest, MixIsHeldUnderTheBusCeiling) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  for (int i = 0; i < 12; ++i) {
    m_backend.play_sound(QStringLiteral("tone"), MiniaudioBackend::MAX_VOLUME, true);
  }
  const std::vector<float> buffer = render(4096);
  EXPECT_GT(peak_of(buffer), 0.1F);
  EXPECT_LE(peak_of(buffer), Game::Audio::BusLimiter::DEFAULT_CEILING + 1e-4F);
}

TEST_F(AudioBackendTest, PlayingAnUnknownTrackIsIgnored) {
  m_backend.play(0, QStringLiteral("missing"), 1.0F, false, 0);
  m_backend.play_sound(QStringLiteral("missing"), 1.0F, false);
  EXPECT_LT(peak_of(render(256)), 1e-6F);
  EXPECT_FALSE(m_backend.any_channel_playing());
}

TEST_F(AudioBackendTest, AFailedDecodeGivesUpItsRegistration) {
  const QString broken = m_directory.filePath("broken.wav");
  {
    QFile file(broken);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_GT(file.write("this is not audio", 17), 0);
  }

  EXPECT_TRUE(m_backend.request_track(
      QStringLiteral("broken"), broken, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  EXPECT_FALSE(m_backend.is_track_ready(QStringLiteral("broken")));
  m_backend.play_sound(QStringLiteral("broken"), 1.0F, false);
  EXPECT_FALSE(m_backend.is_sound_active(QStringLiteral("broken")));

  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("recovered"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();
  EXPECT_TRUE(m_backend.is_track_ready(QStringLiteral("recovered")));
}

TEST_F(AudioBackendTest, UnloadDoesNotWaitOnUnrelatedDecodes) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("first"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_track(QStringLiteral("first"));
  ASSERT_TRUE(m_backend.is_track_ready(QStringLiteral("first")));

  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("second"), m_path, Mastering::Material::Music));
  m_backend.unload(QStringLiteral("first"));
  EXPECT_FALSE(m_backend.is_track_ready(QStringLiteral("first")));

  m_backend.wait_for_decodes();
  EXPECT_TRUE(m_backend.is_track_ready(QStringLiteral("second")));
}

TEST_F(AudioBackendTest, MonoSourcesAreStoredOnceAndStillPlayInStereo) {
  const QString mono_path = m_directory.filePath("mono.wav");
  ASSERT_TRUE(write_tone_wav(mono_path, 0.4F));
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("mono"), mono_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  m_backend.play_sound(QStringLiteral("mono"), 1.0F, false);
  const std::vector<float> buffer = render(1024);
  ASSERT_GT(peak_of(buffer), 0.01F);
  for (std::size_t frame = 0; frame < 1024; ++frame) {
    EXPECT_FLOAT_EQ(buffer[frame * CHANNELS], buffer[(frame * CHANNELS) + 1]);
  }
}

TEST_F(AudioBackendTest, ALoopingBedWrapsWithoutAClick) {
  const int bed_frames = SAMPLE_RATE;
  const QString bed_path = m_directory.filePath("bed.wav");
  ASSERT_TRUE(write_unclosed_bed_wav(bed_path, bed_frames, 0.6F));
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("bed"), bed_path, Mastering::Material::Ambience));
  m_backend.wait_for_decodes();

  m_backend.play_sound(QStringLiteral("bed"), 1.0F, true);

  const unsigned span = static_cast<unsigned>(bed_frames) * 3U;
  const std::vector<float> buffer = render(span);
  ASSERT_GT(peak_of(buffer), 0.01F);

  constexpr unsigned ATTACK_FRAMES = 256;
  std::vector<float> steps;
  steps.reserve(span);
  for (unsigned frame = ATTACK_FRAMES; frame < span; ++frame) {
    steps.push_back(
        std::abs(buffer[frame * CHANNELS] - buffer[(frame - 1) * CHANNELS]));
  }
  std::vector<float> sorted = steps;
  std::sort(sorted.begin(), sorted.end());
  const float median = sorted[sorted.size() / 2];
  const float worst = sorted.back();

  EXPECT_LT(worst, std::max(median * 12.0F, 0.02F))
      << "largest step " << worst << " against a median of " << median;
}

} // namespace

TEST_F(AudioBackendTest, ALoopingBedAskedForMidDecodeStartsWhenItLands) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("rain"), m_path, Mastering::Material::Ambience));

  m_backend.play_sound(QStringLiteral("rain"), 1.0F, true);
  m_backend.wait_for_decodes();

  EXPECT_GT(peak_of(render(1024)), 0.01F)
      << "the weather bed was asked for during the load and never arrived";
  EXPECT_TRUE(m_backend.is_sound_active(QStringLiteral("rain")));
}

TEST_F(AudioBackendTest, ABedStoppedBeforeItsDecodeLandsStaysSilent) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("rain"), m_path, Mastering::Material::Ambience));

  m_backend.play_sound(QStringLiteral("rain"), 1.0F, true);
  m_backend.stop_sound(QStringLiteral("rain"));
  m_backend.wait_for_decodes();

  render(256);
  EXPECT_LT(peak_of(render(1024)), 1e-6F)
      << "a bed cancelled before its decode landed started anyway";
  EXPECT_FALSE(m_backend.is_sound_active(QStringLiteral("rain")));
}

TEST_F(AudioBackendTest, AOneShotAskedForMidDecodeIsNotPlayedLate) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("clang"), m_path, Mastering::Material::Effect));

  m_backend.play_sound(QStringLiteral("clang"), 1.0F, false);
  m_backend.wait_for_decodes();

  EXPECT_LT(peak_of(render(1024)), 1e-6F)
      << "a one-shot fired seconds after the moment it belonged to";
  EXPECT_FALSE(m_backend.is_sound_active(QStringLiteral("clang")));
}

TEST_F(AudioBackendTest, ReloadingAClipDoesNotAnalyseItAgain) {
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();
  const auto after_first_load = m_backend.mastering_analyses_computed();
  EXPECT_EQ(after_first_load, 1U);

  m_backend.unload(QStringLiteral("tone"));
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  EXPECT_TRUE(m_backend.is_track_ready(QStringLiteral("tone")));
  EXPECT_EQ(m_backend.mastering_analyses_computed(), after_first_load)
      << "the same clip was analysed from scratch on its second load";

  m_backend.play_sound(QStringLiteral("tone"), 1.0F, false);
  EXPECT_GT(peak_of(render(256)), 0.01F);
}

TEST_F(AudioBackendTest, DecodedTrackBytesAreCountedAndReleased) {
  EXPECT_GT(m_backend.pcm_budget_bytes(), 0U);
  EXPECT_EQ(m_backend.resident_pcm_bytes(), 0U);

  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  const std::uint64_t resident = m_backend.resident_pcm_bytes();
  EXPECT_GT(resident, 0U);
  EXPECT_GE(m_backend.peak_pcm_bytes(), resident);
  EXPECT_EQ(m_backend.pcm_budget_overruns(), 0U);

  m_backend.unload(QStringLiteral("tone"));

  EXPECT_EQ(m_backend.resident_pcm_bytes(), 0U);
  EXPECT_GE(m_backend.peak_pcm_bytes(), resident);
}

TEST_F(AudioBackendTest, ATightResidencyBudgetIsReportedRatherThanIgnored) {
  m_backend.set_pcm_budget_bytes(1);

  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("tone"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();

  EXPECT_TRUE(m_backend.is_track_ready(QStringLiteral("tone")))
      << "the budget reports pressure; it must not silently drop audio";
  EXPECT_GT(m_backend.pcm_budget_overruns(), 0U);
}

TEST_F(AudioBackendTest, CriticalInformationDucksAlreadyPlayingMusicAndRecovers) {
  using Game::Audio::MixBus;
  m_backend.set_listening_preset(Game::Audio::ListeningPreset::Headphones);
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("bed"), m_path, Mastering::Material::Effect));
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("alert"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();
  m_backend.play(0, QStringLiteral("bed"), 0.5F, true, 0);
  render(SAMPLE_RATE);
  const float baseline = peak_of(render(4096));
  ASSERT_GT(baseline, 0.01F);
  m_backend.play_sound(QStringLiteral("alert"), 0.01F, true, 0.0F, MixBus::Alert, 7);
  render(SAMPLE_RATE);
  const float ducked = peak_of(render(4096));
  EXPECT_LT(ducked, baseline * 0.8F);
  EXPECT_GT(ducked, baseline * 0.5F);
  m_backend.stop_sound(QStringLiteral("alert"));
  render(SAMPLE_RATE * 2);
  EXPECT_NEAR(peak_of(render(4096)), baseline, 0.001F);
}

TEST_F(AudioBackendTest, MutedCriticalVoicesDoNotDuckTheBattle) {
  using Game::Audio::MixBus;
  m_backend.set_listening_preset(Game::Audio::ListeningPreset::Headphones);
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("bed"), m_path, Mastering::Material::Effect));
  ASSERT_TRUE(m_backend.request_track(
      QStringLiteral("voice"), m_path, Mastering::Material::Effect));
  m_backend.wait_for_decodes();
  m_backend.play_sound(QStringLiteral("bed"), 0.5F, true, 0.0F, MixBus::Combat);
  render(SAMPLE_RATE);
  const float baseline = peak_of(render(4096));
  m_backend.play_sound(QStringLiteral("voice"), 0.0F, true, 0.0F, MixBus::Voice, 9);
  render(SAMPLE_RATE);
  EXPECT_NEAR(peak_of(render(4096)), baseline, 0.001F);
}
