#pragma once

#include <QImage>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace Render::Profiling {

struct FrameContinuityConfig {
  int sample_stride{4};
  std::uint64_t warmup_frames{8};
  std::uint64_t maximum_flash_frames{2};
  float luminance_jump{0.12F};
  float affected_pixel_fraction{0.45F};
  float return_tolerance{0.06F};
};

struct FrameContinuityIssue {
  std::uint64_t bright_frame{0};
  std::uint64_t recovery_frame{0};
  float mean_luminance_jump{0.0F};
  float brightened_pixel_fraction{0.0F};
  float recovered_pixel_fraction{0.0F};

  [[nodiscard]] auto message() const -> QString;
};

// Detects transient scene-wide bright frames. Local effects and ordinary
// animation may change many pixels, so a failure requires a broad luminance
// jump followed by a return to the pre-jump image within a short window.
class FrameContinuityAnalyzer {
public:
  explicit FrameContinuityAnalyzer(FrameContinuityConfig config = {});

  void reset();
  [[nodiscard]] auto
  observe(const QImage& frame) -> std::optional<FrameContinuityIssue>;
  [[nodiscard]] auto observed_frames() const noexcept -> std::uint64_t {
    return m_frame_index;
  }

private:
  struct Signature {
    std::vector<std::uint8_t> luminance;
    float mean{0.0F};
  };

  struct PendingFlash {
    Signature baseline;
    Signature bright;
    std::uint64_t frame{0};
    float brightened_fraction{0.0F};
  };

  [[nodiscard]] auto signature(const QImage& frame) const -> Signature;
  [[nodiscard]] static auto fraction_brighter(const Signature& lhs,
                                              const Signature& rhs,
                                              float threshold) -> float;

  FrameContinuityConfig m_config;
  std::uint64_t m_frame_index{0};
  std::optional<Signature> m_previous;
  std::optional<PendingFlash> m_pending;
};

} // namespace Render::Profiling
