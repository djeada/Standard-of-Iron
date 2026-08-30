#include <QImage>
#include <QString>
#include <QTemporaryDir>

#include <algorithm>
#include <gtest/gtest.h>

#include "tools/arena/video_encoder.h"

namespace {

TEST(ArenaVideoEncoderTest, PendingFramesStayBoundedWithoutAnEventLoop) {
  using Arena::Promo::VideoEncoder;
  if (!VideoEncoder::ffmpeg_available()) {
    GTEST_SKIP() << "ffmpeg is not on PATH";
  }

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  constexpr int k_width = 1280;
  constexpr int k_height = 720;
  constexpr int k_frames = 90;
  const qint64 frame_bytes = static_cast<qint64>(k_width) * k_height * 4;
  constexpr int k_tolerated_pending_frames = 8;
  const qint64 cap = frame_bytes * k_tolerated_pending_frames;
  ASSERT_LE(VideoEncoder::max_pending_frames(), k_tolerated_pending_frames);

  VideoEncoder encoder;
  QString error;
  ASSERT_TRUE(encoder.open(
      dir.filePath(QStringLiteral("bounded.mp4")), k_width, k_height, 60, &error))
      << error.toStdString();

  QImage frame(k_width, k_height, QImage::Format_RGBA8888);
  qint64 peak_pending = 0;
  for (int index = 0; index < k_frames; ++index) {
    frame.fill(QColor(index * 2 % 255, 90, 40));
    ASSERT_TRUE(encoder.write_frame(frame, &error)) << error.toStdString();
    peak_pending = std::max(peak_pending, encoder.pending_bytes());
    ASSERT_LE(encoder.pending_bytes(), cap)
        << "frame " << index << " left the ffmpeg pipe buffer unbounded";
  }
  EXPECT_LT(peak_pending, frame_bytes * k_frames);
  ASSERT_TRUE(encoder.close(&error)) << error.toStdString();
  EXPECT_EQ(encoder.frames_written(), k_frames);
}

} // namespace
