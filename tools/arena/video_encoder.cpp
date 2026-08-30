#include "video_encoder.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>

namespace Arena::Promo {
namespace {

constexpr qint64 k_max_buffered_frames = 4;
constexpr int k_pipe_timeout_ms = 30'000;

} // namespace

struct VideoEncoder::Impl {
  QProcess process;
  int width{0};
  int height{0};
  int frames{0};
  bool open{false};
};

VideoEncoder::VideoEncoder()
    : m_impl(std::make_unique<Impl>()) {
}

VideoEncoder::~VideoEncoder() {
  if (m_impl->open) {
    QString ignored;
    (void)close(&ignored);
  }
}

auto VideoEncoder::ffmpeg_available() -> bool {
  return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
}

auto VideoEncoder::open(const QString& output_path,
                        int width,
                        int height,
                        int fps,
                        QString* error) -> bool {
  if (m_impl->open) {
    if (error != nullptr) {
      *error = QStringLiteral("encoder is already open");
    }
    return false;
  }
  const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if (ffmpeg.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg was not found on PATH");
    }
    return false;
  }
  if (!QDir().mkpath(QFileInfo(output_path).absolutePath())) {
    if (error != nullptr) {
      *error =
          QStringLiteral("could not create output directory for %1").arg(output_path);
    }
    return false;
  }

  m_impl->width = width;
  m_impl->height = height;
  m_impl->frames = 0;

  // Shot clips stay near-lossless because the edit pass re-encodes them after
  // grading; quality lost here would be baked into the final short.
  const QStringList arguments{QStringLiteral("-hide_banner"),
                              QStringLiteral("-loglevel"),
                              QStringLiteral("error"),
                              QStringLiteral("-y"),
                              QStringLiteral("-f"),
                              QStringLiteral("rawvideo"),
                              QStringLiteral("-pixel_format"),
                              QStringLiteral("rgba"),
                              QStringLiteral("-video_size"),
                              QStringLiteral("%1x%2").arg(width).arg(height),
                              QStringLiteral("-framerate"),
                              QString::number(fps),
                              QStringLiteral("-i"),
                              QStringLiteral("-"),
                              QStringLiteral("-an"),
                              QStringLiteral("-c:v"),
                              QStringLiteral("libx264"),
                              QStringLiteral("-preset"),
                              QStringLiteral("veryfast"),
                              QStringLiteral("-crf"),
                              QStringLiteral("14"),
                              QStringLiteral("-pix_fmt"),
                              QStringLiteral("yuv420p"),
                              QStringLiteral("-movflags"),
                              QStringLiteral("+faststart"),
                              output_path};

  m_impl->process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
  m_impl->process.start(ffmpeg, arguments);
  if (!m_impl->process.waitForStarted(k_pipe_timeout_ms)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not start ffmpeg: %1")
                   .arg(m_impl->process.errorString());
    }
    return false;
  }
  m_impl->open = true;
  return true;
}

auto VideoEncoder::write_frame(const QImage& frame, QString* error) -> bool {
  if (!m_impl->open) {
    if (error != nullptr) {
      *error = QStringLiteral("encoder is not open");
    }
    return false;
  }
  if (frame.width() != m_impl->width || frame.height() != m_impl->height) {
    if (error != nullptr) {
      *error = QStringLiteral("frame %1x%2 does not match the encoder size %3x%4")
                   .arg(frame.width())
                   .arg(frame.height())
                   .arg(m_impl->width)
                   .arg(m_impl->height);
    }
    return false;
  }

  const QImage rgba = frame.format() == QImage::Format_RGBA8888
                          ? frame
                          : frame.convertToFormat(QImage::Format_RGBA8888);
  for (int row = 0; row < rgba.height(); ++row) {
    const char* line = reinterpret_cast<const char*>(rgba.constScanLine(row));
    qint64 remaining = static_cast<qint64>(rgba.width()) * 4;
    while (remaining > 0) {
      const qint64 written = m_impl->process.write(line, remaining);
      if (written < 0) {
        if (error != nullptr) {
          *error = QStringLiteral("ffmpeg pipe write failed: %1")
                       .arg(m_impl->process.errorString());
        }
        return false;
      }
      line += written;
      remaining -= written;
      if (remaining > 0 && !m_impl->process.waitForBytesWritten(k_pipe_timeout_ms)) {
        if (error != nullptr) {
          *error = QStringLiteral("ffmpeg stopped accepting frames: %1")
                       .arg(m_impl->process.errorString());
        }
        return false;
      }
    }
  }
  const qint64 frame_bytes = static_cast<qint64>(rgba.width()) * rgba.height() * 4;
  while (m_impl->process.bytesToWrite() > frame_bytes * k_max_buffered_frames) {
    if (!m_impl->process.waitForBytesWritten(k_pipe_timeout_ms)) {
      if (error != nullptr) {
        *error = QStringLiteral("ffmpeg fell behind and stopped draining frames: %1")
                     .arg(m_impl->process.errorString());
      }
      return false;
    }
  }
  ++m_impl->frames;
  return true;
}

auto VideoEncoder::pending_bytes() const -> qint64 {
  return m_impl->open ? m_impl->process.bytesToWrite() : 0;
}

auto VideoEncoder::max_pending_frames() noexcept -> int {
  return static_cast<int>(k_max_buffered_frames);
}

auto VideoEncoder::close(QString* error) -> bool {
  if (!m_impl->open) {
    return true;
  }
  m_impl->open = false;
  m_impl->process.closeWriteChannel();
  if (!m_impl->process.waitForFinished(k_pipe_timeout_ms)) {
    m_impl->process.kill();
    m_impl->process.waitForFinished(5'000);
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg did not finish encoding in time");
    }
    return false;
  }
  if (m_impl->process.exitStatus() != QProcess::NormalExit ||
      m_impl->process.exitCode() != 0) {
    if (error != nullptr) {
      *error =
          QStringLiteral("ffmpeg exited with code %1").arg(m_impl->process.exitCode());
    }
    return false;
  }
  return true;
}

auto VideoEncoder::frames_written() const noexcept -> int {
  return m_impl->frames;
}

} // namespace Arena::Promo
