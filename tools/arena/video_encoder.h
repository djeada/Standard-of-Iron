#pragma once

#include <QString>

#include <memory>

class QImage;

namespace Arena::Promo {

class VideoEncoder {
public:
  VideoEncoder();
  ~VideoEncoder();

  VideoEncoder(const VideoEncoder&) = delete;
  auto operator=(const VideoEncoder&) -> VideoEncoder& = delete;
  VideoEncoder(VideoEncoder&&) = delete;
  auto operator=(VideoEncoder&&) -> VideoEncoder& = delete;

  [[nodiscard]] static auto ffmpeg_available() -> bool;

  [[nodiscard]] auto open(const QString& output_path,
                          int width,
                          int height,
                          int fps,
                          QString* error) -> bool;

  [[nodiscard]] auto write_frame(const QImage& frame, QString* error) -> bool;
  [[nodiscard]] auto close(QString* error) -> bool;
  [[nodiscard]] auto frames_written() const noexcept -> int;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace Arena::Promo
