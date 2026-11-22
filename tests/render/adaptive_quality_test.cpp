#include <QString>

#include <gtest/gtest.h>
#include <optional>
#include <utility>

#include "render/backend/adapter_class.h"
#include "render/frame/graphics_settings.h"
#include "render/scene/adaptive_quality_governor.h"

namespace {

using Render::AdaptiveQualityGovernor;
using Render::GraphicsQuality;

// Feeds `seconds` of frames at `frame_ms` and returns the first step down
// the governor asks for, if any.
auto run_frames(AdaptiveQualityGovernor& governor,
                double frame_ms,
                double seconds,
                GraphicsQuality quality,
                bool eligible = true,
                bool chosen_by_user = false) -> std::optional<GraphicsQuality> {
  const int frames = static_cast<int>(seconds * 1000.0 / frame_ms);
  for (int i = 0; i < frames; ++i) {
    if (auto lower = governor.observe(frame_ms, eligible, quality, chosen_by_user)) {
      return lower;
    }
  }
  return std::nullopt;
}

TEST(AdaptiveQualityGovernorTest, ASmoothBattleKeepsItsPreset) {
  AdaptiveQualityGovernor governor;
  EXPECT_FALSE(run_frames(governor, 16.7, 60.0, GraphicsQuality::High).has_value());
}

TEST(AdaptiveQualityGovernorTest, ASlowBattleStepsDownOnePreset) {
  AdaptiveQualityGovernor governor;
  const auto lower = run_frames(governor, 70.0, 20.0, GraphicsQuality::High);
  ASSERT_TRUE(lower.has_value());
  EXPECT_EQ(*lower, GraphicsQuality::Medium);
}

TEST(AdaptiveQualityGovernorTest, ItWaitsOutTheWarmUpBeforeJudging) {
  AdaptiveQualityGovernor governor;
  const auto& policy = governor.policy();
  const double too_early = policy.warmup_seconds + policy.window_seconds - 1.0;
  EXPECT_FALSE(run_frames(governor, 70.0, too_early, GraphicsQuality::High).has_value())
      << "first-use uploads right after the loading screen are not the frame rate";
}

TEST(AdaptiveQualityGovernorTest, OccasionalHitchesDoNotLowerTheGraphics) {
  AdaptiveQualityGovernor governor;
  for (int i = 0; i < 4000; ++i) {
    const double frame_ms = (i % 20 == 0) ? 900.0 : 16.7;
    ASSERT_FALSE(
        governor.observe(frame_ms, true, GraphicsQuality::High, false).has_value())
        << "an autosave or shader compile hitch is not a slow battle";
  }
}

TEST(AdaptiveQualityGovernorTest, ThePlayersOwnChoiceIsNeverOverridden) {
  AdaptiveQualityGovernor governor;
  EXPECT_FALSE(run_frames(governor, 120.0, 60.0, GraphicsQuality::Ultra, true, true)
                   .has_value());
}

TEST(AdaptiveQualityGovernorTest, LowIsTheFloor) {
  AdaptiveQualityGovernor governor;
  EXPECT_FALSE(run_frames(governor, 120.0, 60.0, GraphicsQuality::Low).has_value());
}

TEST(AdaptiveQualityGovernorTest, FramesBehindTheLoadingScreenRestartTheJudgement) {
  AdaptiveQualityGovernor governor;
  const auto& policy = governor.policy();
  const double almost = policy.warmup_seconds + policy.window_seconds * 0.9;
  EXPECT_FALSE(run_frames(governor, 70.0, almost, GraphicsQuality::High).has_value());
  EXPECT_FALSE(governor.observe(70.0, false, GraphicsQuality::High, false).has_value());
  EXPECT_FALSE(run_frames(governor, 70.0, almost, GraphicsQuality::High).has_value())
      << "an ineligible frame must restart the warm-up, not resume the window";
}

TEST(AdaptiveQualityGovernorTest, AfterAStepItWarmsUpAgainBeforeTheNextOne) {
  AdaptiveQualityGovernor governor;
  ASSERT_TRUE(run_frames(governor, 70.0, 20.0, GraphicsQuality::Ultra).has_value());
  const auto& policy = governor.policy();
  EXPECT_FALSE(run_frames(governor,
                          70.0,
                          policy.warmup_seconds + policy.window_seconds - 1.0,
                          GraphicsQuality::High)
                   .has_value());
  const auto next = run_frames(governor, 70.0, 20.0, GraphicsQuality::High);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(*next, GraphicsQuality::Medium);
}

TEST(RenderScaleTest, HighAndUltraDrawEveryPixel) {
  for (auto quality : {GraphicsQuality::High, GraphicsQuality::Ultra}) {
    const auto& settings = Render::graphics_profile_for(quality).render_scale;
    const auto extent = Render::scaled_render_extent(settings, 3840, 2160);
    EXPECT_EQ(extent[0], 3840);
    EXPECT_EQ(extent[1], 2160);
  }
}

TEST(RenderScaleTest, LowDrawsAtMostAboutAMegapixel) {
  const auto& settings =
      Render::graphics_profile_for(GraphicsQuality::Low).render_scale;
  for (auto [w, h] :
       {std::pair{1920, 1080}, std::pair{2560, 1440}, std::pair{3840, 2160}}) {
    const auto extent = Render::scaled_render_extent(settings, w, h);
    EXPECT_LE(static_cast<long>(extent[0]) * extent[1], 1'010'000L) << w << "x" << h;
    EXPECT_NEAR(
        static_cast<double>(extent[0]) / extent[1], static_cast<double>(w) / h, 0.01)
        << "the picture must keep its aspect ratio";
  }
}

TEST(RenderScaleTest, LowStillScalesDownASmallWindow) {
  const auto& settings =
      Render::graphics_profile_for(GraphicsQuality::Low).render_scale;
  const auto extent = Render::scaled_render_extent(settings, 1280, 720);
  EXPECT_EQ(extent[0], 1024);
  EXPECT_EQ(extent[1], 576);
}

TEST(RenderScaleTest, MediumCapsHiDpiPanelsAtAboutFullHd) {
  const auto& settings =
      Render::graphics_profile_for(GraphicsQuality::Medium).render_scale;
  const auto full_hd = Render::scaled_render_extent(settings, 1920, 1080);
  EXPECT_EQ(full_hd[0], 1920);
  EXPECT_EQ(full_hd[1], 1080);
  const auto uhd = Render::scaled_render_extent(settings, 3840, 2160);
  EXPECT_LE(static_cast<long>(uhd[0]) * uhd[1], 2'110'000L);
}

TEST(RenderScaleTest, ADegenerateWindowStaysValid) {
  const auto& settings =
      Render::graphics_profile_for(GraphicsQuality::Low).render_scale;
  const auto extent = Render::scaled_render_extent(settings, 1, 1);
  EXPECT_EQ(extent[0], 1);
  EXPECT_EQ(extent[1], 1);
}

TEST(AdapterClassTest, RecognisesCommonGpus) {
  using Render::GL::AdapterClass;
  using Render::GL::classify_adapter;
  struct Case {
    const char* renderer;
    AdapterClass expected;
  };
  const Case cases[] = {
      {"llvmpipe (LLVM 20.1.2, 256 bits)", AdapterClass::Software},
      {"Microsoft Basic Render Driver", AdapterClass::Software},
      {"Intel(R) UHD Graphics 620", AdapterClass::BasicIntegrated},
      {"Intel(R) HD Graphics 520", AdapterClass::BasicIntegrated},
      {"Mesa Intel(R) UHD Graphics 630 (CFL GT2)", AdapterClass::BasicIntegrated},
      {"Mesa Intel(R) Graphics (ADL GT2)", AdapterClass::BasicIntegrated},
      {"Intel(R) Iris(R) Xe Graphics", AdapterClass::CapableIntegrated},
      {"Intel(R) Arc(TM) Graphics", AdapterClass::CapableIntegrated},
      {"Intel(R) Arc(TM) A770 Graphics", AdapterClass::Discrete},
      {"AMD Radeon(TM) Graphics", AdapterClass::CapableIntegrated},
      {"AMD Radeon(TM) Vega 8 Graphics", AdapterClass::CapableIntegrated},
      {"AMD Radeon Graphics (radeonsi, renoir, LLVM 15.0.7, DRM 3.54)",
       AdapterClass::CapableIntegrated},
      {"AMD Radeon 780M (radeonsi, phoenix, ACO, DRM 3.57)",
       AdapterClass::CapableIntegrated},
      {"AMD Radeon RX 6600 (radeonsi, navi23, LLVM 15.0.7)", AdapterClass::Discrete},
      {"Radeon RX 580 Series", AdapterClass::Discrete},
      {"NVIDIA GeForce MX250/PCIe/SSE2", AdapterClass::CapableIntegrated},
      {"NVIDIA GeForce RTX 5060/PCIe/SSE2", AdapterClass::Discrete},
      {"Apple M2", AdapterClass::Discrete},
  };
  for (const auto& c : cases) {
    EXPECT_EQ(classify_adapter(QString::fromLatin1(c.renderer)), c.expected)
        << c.renderer;
  }
}

TEST(AdapterClassTest, IntegratedGpusStartBelowHigh) {
  using Render::GL::AdapterClass;
  using Render::GL::first_run_quality_cap;
  EXPECT_EQ(first_run_quality_cap(AdapterClass::Software), GraphicsQuality::Low);
  EXPECT_EQ(first_run_quality_cap(AdapterClass::BasicIntegrated), GraphicsQuality::Low);
  EXPECT_EQ(first_run_quality_cap(AdapterClass::CapableIntegrated),
            GraphicsQuality::Medium);
  EXPECT_GE(first_run_quality_cap(AdapterClass::Discrete),
            Render::k_default_graphics_quality);
}

TEST(GraphicsSettingsAutoLowerTest, StepsDownOnlyWithoutAPlayerChoice) {
  auto& graphics = Render::GraphicsSettings::instance();
  const auto saved_quality = graphics.quality();
  const bool saved_choice = graphics.quality_chosen_by_user();

  graphics.set_quality(GraphicsQuality::High);
  graphics.set_quality_chosen_by_user(false);
  const auto before = graphics.auto_lowered_count();
  EXPECT_TRUE(graphics.lower_quality_automatically(GraphicsQuality::Medium));
  EXPECT_EQ(graphics.quality(), GraphicsQuality::Medium);
  EXPECT_EQ(graphics.auto_lowered_count(), before + 1U);

  EXPECT_FALSE(graphics.lower_quality_automatically(GraphicsQuality::High))
      << "the automatic path never raises the preset";

  graphics.set_quality_chosen_by_user(true);
  EXPECT_FALSE(graphics.lower_quality_automatically(GraphicsQuality::Low));
  EXPECT_EQ(graphics.quality(), GraphicsQuality::Medium);

  graphics.set_quality_chosen_by_user(saved_choice);
  graphics.set_quality(saved_quality);
}

} // namespace
