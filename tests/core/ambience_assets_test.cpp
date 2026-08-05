#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "game/audio/miniaudio_backend.h"

namespace {

constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;

constexpr const char* AMBIENCE_DIR = "assets/audio/ambience";

auto vorbis_sample_rate(const QString& path) -> int {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return 0;
  }
  const QByteArray head = file.read(4096);
  const QByteArray marker = QByteArray("\x01vorbis", 7);
  const int at = head.indexOf(marker);
  if (at < 0 || at + 16 > head.size()) {
    return 0;
  }
  const auto* bytes =
      reinterpret_cast<const unsigned char*>(head.constData()) + at + 12;
  return int(bytes[0]) | (int(bytes[1]) << 8) | (int(bytes[2]) << 16) |
         (int(bytes[3]) << 24);
}

auto tone_power(const std::vector<float>& pcm, double hz) -> double {
  const std::size_t frames = pcm.size() / CHANNELS;
  const double omega = 2.0 * std::numbers::pi * hz / SAMPLE_RATE;
  const double coefficient = 2.0 * std::cos(omega);
  double s1 = 0.0;
  double s2 = 0.0;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const double s0 = pcm[frame * CHANNELS] + coefficient * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  const double power = (s1 * s1) + (s2 * s2) - (coefficient * s1 * s2);
  return std::max(0.0, power) / double(frames * frames);
}

auto band_db(const std::vector<float>& pcm, double from_hz, double to_hz) -> float {
  double total = 0.0;
  int bins = 0;
  for (double hz = from_hz; hz <= to_hz; hz += 200.0) {
    total += tone_power(pcm, hz);
    ++bins;
  }
  return bins == 0 ? -999.0F
                   : static_cast<float>(10.0 * std::log10(total / bins + 1.0e-20));
}

auto ambience_files() -> QStringList {
  QDir dir(QString::fromLatin1(AMBIENCE_DIR));
  return dir.entryList({QStringLiteral("*.ogg")}, QDir::Files, QDir::Name);
}

class AmbienceAssetsTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(QFileInfo::exists(QString::fromLatin1(AMBIENCE_DIR)))
        << "run the suite from the repository root";
    if (ambience_files().isEmpty()) {
      GTEST_SKIP() << "no beds rendered: this build had no Python or no ffmpeg";
    }
  }
};

TEST_F(AmbienceAssetsTest, EveryBedIsStoredAtTheMixerSampleRate) {
  const QStringList files = ambience_files();

  for (const QString& name : files) {
    const QString path = QString::fromLatin1(AMBIENCE_DIR) + QLatin1Char('/') + name;
    EXPECT_EQ(vorbis_sample_rate(path), SAMPLE_RATE) << name.toStdString();
  }
}

TEST_F(AmbienceAssetsTest, NoBedIsLouderInTheHarshBandThanInItsBody) {
  const QStringList files = ambience_files();

  MiniaudioBackend backend;
  ASSERT_TRUE(backend.initialize(
      SAMPLE_RATE, CHANNELS, MiniaudioBackend::DEFAULT_MUSIC_CHANNELS, false));

  for (const QString& name : files) {
    const QString path = QString::fromLatin1(AMBIENCE_DIR) + QLatin1Char('/') + name;
    const QString id = QStringLiteral("bed_") + name;
    ASSERT_TRUE(
        backend.request_track(id, path, Game::Audio::Mastering::Material::Ambience))
        << name.toStdString();
    backend.wait_for_decodes();
    backend.play_sound(id, 1.0F, true);

    const unsigned span = SAMPLE_RATE * 2;
    std::vector<float> buffer(std::size_t(span) * CHANNELS, 0.0F);
    backend.on_audio(buffer.data(), span);

    const float body = band_db(buffer, 100.0, 800.0);
    const float harsh = band_db(buffer, 2000.0, 6000.0);
    EXPECT_LT(harsh, body - 3.0F) << name.toStdString() << ": harsh band " << harsh
                                  << " dB against a body of " << body << " dB";

    backend.stop_sound(id);
    backend.unload(id);
  }
  backend.shutdown();
}

} // namespace
