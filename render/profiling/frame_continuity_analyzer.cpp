#include "frame_continuity_analyzer.h"

#include <algorithm>
#include <cmath>

namespace Render::Profiling {
namespace {

constexpr float k_byte_scale = 1.0F / 255.0F;

auto byte_luminance(const uchar* rgba) noexcept -> std::uint8_t {

  return static_cast<std::uint8_t>((54U * rgba[0] + 183U * rgba[1] + 19U * rgba[2]) >>
                                   8U);
}

} // namespace

auto FrameContinuityIssue::message() const -> QString {
  return QStringLiteral("full-screen luminance flash at frame %1 recovered at frame %2 "
                        "(mean jump %3, brightened %4%, recovered %5%)")
      .arg(bright_frame)
      .arg(recovery_frame)
      .arg(mean_luminance_jump, 0, 'f', 3)
      .arg(brightened_pixel_fraction * 100.0F, 0, 'f', 1)
      .arg(recovered_pixel_fraction * 100.0F, 0, 'f', 1);
}

FrameContinuityAnalyzer::FrameContinuityAnalyzer(FrameContinuityConfig config)
    : m_config(config) {
  m_config.sample_stride = std::max(1, m_config.sample_stride);
  m_config.maximum_flash_frames =
      std::max<std::uint64_t>(1U, m_config.maximum_flash_frames);
  m_config.luminance_jump = std::clamp(m_config.luminance_jump, 0.01F, 1.0F);
  m_config.affected_pixel_fraction =
      std::clamp(m_config.affected_pixel_fraction, 0.01F, 1.0F);
  m_config.return_tolerance = std::clamp(m_config.return_tolerance, 0.0F, 1.0F);
}

void FrameContinuityAnalyzer::reset() {
  m_frame_index = 0;
  m_previous.reset();
  m_pending.reset();
}

auto FrameContinuityAnalyzer::signature(const QImage& frame) const -> Signature {
  Signature result;
  if (frame.isNull()) {
    return result;
  }

  const QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
  const int stride = m_config.sample_stride;
  const int offset = stride / 2;
  const int sample_columns = std::max(0, (rgba.width() - offset + stride - 1) / stride);
  const int sample_rows = std::max(0, (rgba.height() - offset + stride - 1) / stride);
  result.luminance.reserve(static_cast<std::size_t>(sample_columns * sample_rows));

  std::uint64_t sum = 0;
  for (int y = offset; y < rgba.height(); y += stride) {
    const uchar* row = rgba.constScanLine(y);
    for (int x = offset; x < rgba.width(); x += stride) {
      const auto luminance = byte_luminance(row + x * 4);
      result.luminance.push_back(luminance);
      sum += luminance;
    }
  }
  if (!result.luminance.empty()) {
    result.mean = static_cast<float>(sum) * k_byte_scale /
                  static_cast<float>(result.luminance.size());
  }
  return result;
}

auto FrameContinuityAnalyzer::fraction_brighter(const Signature& lhs,
                                                const Signature& rhs,
                                                float threshold) -> float {
  const std::size_t count = std::min(lhs.luminance.size(), rhs.luminance.size());
  if (count == 0U) {
    return 0.0F;
  }
  const int byte_threshold =
      std::max(1, static_cast<int>(std::lround(threshold * 255.0F)));
  std::size_t brighter = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (static_cast<int>(lhs.luminance[index]) -
            static_cast<int>(rhs.luminance[index]) >=
        byte_threshold) {
      ++brighter;
    }
  }
  return static_cast<float>(brighter) / static_cast<float>(count);
}

auto FrameContinuityAnalyzer::observe(const QImage& frame)
    -> std::optional<FrameContinuityIssue> {
  ++m_frame_index;
  Signature current = signature(frame);
  if (current.luminance.empty()) {
    return std::nullopt;
  }

  std::optional<FrameContinuityIssue> issue;
  if (m_pending.has_value()) {
    const std::uint64_t age = m_frame_index - m_pending->frame;
    const float recovered_fraction =
        fraction_brighter(m_pending->bright, current, m_config.luminance_jump * 0.65F);
    const bool mean_recovered =
        std::abs(current.mean - m_pending->baseline.mean) <= m_config.return_tolerance;
    if (age <= m_config.maximum_flash_frames && mean_recovered &&
        recovered_fraction >= m_config.affected_pixel_fraction) {
      issue = FrameContinuityIssue{
          .bright_frame = m_pending->frame,
          .recovery_frame = m_frame_index,
          .mean_luminance_jump = m_pending->bright.mean - m_pending->baseline.mean,
          .brightened_pixel_fraction = m_pending->brightened_fraction,
          .recovered_pixel_fraction = recovered_fraction,
      };
      m_pending.reset();
    } else if (age >= m_config.maximum_flash_frames) {
      m_pending.reset();
    }
  }

  if (!issue.has_value() && !m_pending.has_value() && m_previous.has_value() &&
      m_frame_index > m_config.warmup_frames &&
      current.luminance.size() == m_previous->luminance.size()) {
    const float mean_jump = current.mean - m_previous->mean;
    const float brightened_fraction =
        fraction_brighter(current, *m_previous, m_config.luminance_jump);
    if (mean_jump >= m_config.luminance_jump &&
        brightened_fraction >= m_config.affected_pixel_fraction) {
      m_pending = PendingFlash{
          .baseline = *m_previous,
          .bright = current,
          .frame = m_frame_index,
          .brightened_fraction = brightened_fraction,
      };
    }
  }

  m_previous = std::move(current);
  return issue;
}

} // namespace Render::Profiling
