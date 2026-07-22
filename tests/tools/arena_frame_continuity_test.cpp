#include <QColor>
#include <QImage>
#include <QPainter>

#include <gtest/gtest.h>

#include "render/profiling/frame_continuity_analyzer.h"

namespace {

auto solid_frame(const QColor& color) -> QImage {
  QImage frame(120, 80, QImage::Format_RGBA8888);
  frame.fill(color);
  return frame;
}

auto test_config() -> Render::Profiling::FrameContinuityConfig {
  return {
      .sample_stride = 1,
      .warmup_frames = 0,
      .maximum_flash_frames = 2,
      .luminance_jump = 0.15F,
      .affected_pixel_fraction = 0.45F,
      .return_tolerance = 0.03F,
  };
}

} // namespace

TEST(ArenaFrameContinuityTest, DetectsSceneWideOneFrameBrightFlash) {
  Render::Profiling::FrameContinuityAnalyzer analyzer(test_config());
  const QImage baseline = solid_frame(QColor(36, 48, 34));
  const QImage flash = solid_frame(QColor(226, 232, 220));

  EXPECT_FALSE(analyzer.observe(baseline).has_value());
  EXPECT_FALSE(analyzer.observe(flash).has_value());
  const auto issue = analyzer.observe(baseline);

  ASSERT_TRUE(issue.has_value());
  EXPECT_EQ(issue->bright_frame, 2U);
  EXPECT_EQ(issue->recovery_frame, 3U);
  EXPECT_GT(issue->brightened_pixel_fraction, 0.99F);
  EXPECT_GT(issue->recovered_pixel_fraction, 0.99F);
}

TEST(ArenaFrameContinuityTest, IgnoresLocalBrightCombatEffect) {
  Render::Profiling::FrameContinuityAnalyzer analyzer(test_config());
  const QImage baseline = solid_frame(QColor(36, 48, 34));
  QImage local_effect = baseline;
  {
    QPainter painter(&local_effect);
    painter.fillRect(QRect(45, 25, 30, 30), QColor(255, 220, 120));
  }

  EXPECT_FALSE(analyzer.observe(baseline).has_value());
  EXPECT_FALSE(analyzer.observe(local_effect).has_value());
  EXPECT_FALSE(analyzer.observe(baseline).has_value());
}

TEST(ArenaFrameContinuityTest, IgnoresSustainedLightingChange) {
  Render::Profiling::FrameContinuityAnalyzer analyzer(test_config());
  const QImage baseline = solid_frame(QColor(36, 48, 34));
  const QImage daylight = solid_frame(QColor(205, 210, 195));

  EXPECT_FALSE(analyzer.observe(baseline).has_value());
  EXPECT_FALSE(analyzer.observe(daylight).has_value());
  EXPECT_FALSE(analyzer.observe(daylight).has_value());
  EXPECT_FALSE(analyzer.observe(daylight).has_value());
  EXPECT_FALSE(analyzer.observe(baseline).has_value());
}
