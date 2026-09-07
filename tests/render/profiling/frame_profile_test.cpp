

#include <gtest/gtest.h>
#include <limits>
#include <thread>

#include "render/profiling/frame_pacing.h"
#include "render/profiling/frame_profile.h"
#include "render/profiling/presentation_cycle.h"

using Render::Profiling::format_overlay;
using Render::Profiling::FrameProfile;
using Render::Profiling::Phase;
using Render::Profiling::PhaseScope;

TEST(FrameProfileTest, ResetZeroes) {
  FrameProfile p;
  p.enabled = true;
  p.add_phase_us(Phase::Sort, 123);
  p.draw_calls = 10;
  p.triangles = 5000;
  p.reset();
  EXPECT_EQ(p.total_us(), 0U);
  EXPECT_EQ(p.draw_calls, 0U);
  EXPECT_EQ(p.triangles, 0U);
}

TEST(FrameProfileTest, DisabledProfileIgnoresWrites) {
  FrameProfile p;
  p.enabled = false;
  p.add_phase_us(Phase::Sort, 999);
  EXPECT_EQ(p.total_us(), 0U);
}

TEST(FrameProfileTest, PhaseScopeRecordsElapsed) {
  FrameProfile p;
  p.enabled = true;
  {
    PhaseScope const scope(&p, Phase::Submit);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  auto const us = p.phase_us[static_cast<std::size_t>(Phase::Submit)];
  EXPECT_GE(us, 1000U);
  EXPECT_LT(us, 100000U);
}

TEST(FrameProfileTest, PhaseScopeSkipsWhenDisabled) {
  FrameProfile p;
  p.enabled = false;
  {
    PhaseScope const scope(&p, Phase::Sort);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(p.total_us(), 0U);
}

TEST(FrameProfileTest, TotalSumsAllPhases) {
  FrameProfile p;
  p.enabled = true;
  p.add_phase_us(Phase::Collection, 100);
  p.add_phase_us(Phase::Sort, 200);
  p.add_phase_us(Phase::Playback, 300);
  EXPECT_EQ(p.total_us(), 600U);
}

TEST(FrameProfileTest, FormatOverlayIncludesAllPhases) {
  FrameProfile p;
  p.enabled = true;
  p.frame_index = 42;
  p.add_phase_us(Phase::Collection, 1000);
  p.add_phase_us(Phase::Sort, 500);
  p.add_phase_us(Phase::Playback, 2500);
  p.combat_state_update_us = 250;
  p.animation_input_sampling_us = 500;
  p.humanoid_preparation_us = 750;
  p.bpat_playback_us = 1000;
  p.render_asset_cache_lookup_us = 125;
  p.soldier_layout_generation_us = 375;
  p.visible_soldiers = 64;
  p.render_asset_cache_hits = 120;
  p.render_asset_cache_misses = 8;
  p.draw_calls = 123;
  p.triangles = 4567;
  p.instances = 890;
  p.budget_headroom_ms = 5.5;
  p.finish_frame_sample();

  std::string const s = format_overlay(p);
  EXPECT_NE(s.find("frame #42"), std::string::npos);
  EXPECT_NE(s.find("collect"), std::string::npos);
  EXPECT_NE(s.find("sort"), std::string::npos);
  EXPECT_NE(s.find("play"), std::string::npos);
  EXPECT_NE(s.find("draws=123"), std::string::npos);
  EXPECT_NE(s.find("tris=4567"), std::string::npos);
  EXPECT_NE(s.find("inst=890"), std::string::npos);
  EXPECT_NE(s.find("avg/p50/p95/p99"), std::string::npos);
  EXPECT_NE(s.find("soldiers=64"), std::string::npos);
  EXPECT_NE(s.find("cache h/m=120/8"), std::string::npos);
  EXPECT_NE(s.find("combat"), std::string::npos);
}

TEST(FrameProfileTest, FormatOverlayHandlesZeroTotal) {
  FrameProfile const p;
  std::string const s = format_overlay(p);
  EXPECT_NE(s.find("frame #0"), std::string::npos);
  EXPECT_NE(s.find("total"), std::string::npos);
  EXPECT_NE(s.find("draws="), std::string::npos);
}

TEST(FrameProfileTest, GlobalProfileIsSingleton) {
  auto& a = Render::Profiling::global_profile();
  auto& b = Render::Profiling::global_profile();
  EXPECT_EQ(&a, &b);
  bool const was_enabled = a.enabled;
  a.enabled = true;
  a.reset();
  a.add_phase_us(Phase::Sort, 10);
  EXPECT_EQ(b.phase_us[static_cast<std::size_t>(Phase::Sort)], 10U);
  a.reset();
  a.enabled = was_enabled;
}

TEST(FrameProfileTest, ProfilingIsOffUntilSomethingAsksForIt) {
  FrameProfile const p;
  EXPECT_FALSE(p.enabled);
}

TEST(FrameProfileTest, FinishFrameSampleComputesRollingAverageAndPercentiles) {
  FrameProfile p;
  p.enabled = true;
  p.add_phase_us(Phase::Collection, 1000);
  p.finish_frame_sample();
  p.reset();
  p.add_phase_us(Phase::Collection, 3000);
  p.finish_frame_sample();
  p.reset();
  p.add_phase_us(Phase::Collection, 5000);
  p.finish_frame_sample();

  EXPECT_NEAR(p.average_frame_ms, 3.0, 0.01);
  EXPECT_NEAR(p.p50_frame_ms, 3.0, 0.01);
  EXPECT_NEAR(p.p95_frame_ms, 5.0, 0.01);
  EXPECT_NEAR(p.p99_frame_ms, 5.0, 0.01);
}

TEST(FrameProfileTest, RollingStatsDescribeTheRecentWindowOnly) {
  FrameProfile p;
  p.enabled = true;

  p.add_phase_us(Phase::Collection, 100000);
  p.finish_frame_sample();
  for (int frame = 0; frame < 200; ++frame) {
    p.reset();
    p.add_phase_us(Phase::Collection, 1000);
    p.finish_frame_sample();
  }

  EXPECT_NEAR(p.average_frame_ms, 1.0, 0.01);
  EXPECT_NEAR(p.p99_frame_ms, 1.0, 0.01);
}

TEST(FrameProfileTest, PhaseNameMatchesEnum) {
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Collection), "collect");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Playback), "play");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Present), "present");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Simulation), "sim");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Snapshot), "snapshot");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Shadow), "shadow");
}

TEST(FrameProfileTest, NestedScopesDoNotDoubleCount) {
  FrameProfile p;
  p.enabled = true;
  {
    PhaseScope const outer(&p, Phase::Playback);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    PhaseScope const inner(&p, Phase::Shadow);
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
  }
  auto const play_us = p.phase_us[static_cast<std::size_t>(Phase::Playback)];
  auto const shadow_us = p.phase_us[static_cast<std::size_t>(Phase::Shadow)];
  EXPECT_GE(shadow_us, 3000U);
  EXPECT_GE(play_us, 1000U);
  EXPECT_LT(play_us, shadow_us);
  EXPECT_EQ(p.total_us(), play_us + shadow_us);
}

TEST(FrameProfileTest, BeginFrameResetsAndAdvancesIndex) {
  FrameProfile p;
  p.enabled = true;
  p.add_phase_us(Phase::Sort, 500);
  p.begin_frame();
  EXPECT_TRUE(p.frame_open);
  EXPECT_EQ(p.frame_index, 1U);
  EXPECT_EQ(p.total_us(), 0U);
  p.add_phase_us(Phase::Sort, 700);
  p.end_frame();
  EXPECT_FALSE(p.frame_open);
  EXPECT_EQ(p.phase_us[static_cast<std::size_t>(Phase::Sort)], 700U);
}

TEST(FrameProfileTest, SimulationPhaseSurvivesARendererOpenedFrame) {
  FrameProfile p;
  p.enabled = true;
  p.begin_frame();
  p.add_phase_us(Phase::Simulation, 4000);
  if (!p.frame_open) {
    p.begin_frame();
  }
  p.add_phase_us(Phase::Collection, 1000);
  EXPECT_EQ(p.phase_us[static_cast<std::size_t>(Phase::Simulation)], 4000U);
  EXPECT_EQ(p.total_us(), 5000U);
}

TEST(FramePacingTest, SmoothSixtyHzPasses) {
  Render::Profiling::FramePacing pacing;
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 8, 1024, {}});
  }
  EXPECT_TRUE(pacing.report("high")["passed"].toBool());
}

TEST(FramePacingTest, ConsecutiveHitchesIncludeTrailingClusterAndEvidence) {
  Render::Profiling::FramePacing pacing;
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 8, 0, {}});
  }
  Render::Profiling::PacingSample hitch{45, 40, 8, 0, {}};
  hitch.phase_us[static_cast<std::size_t>(Phase::Submit)] = 35000;
  pacing.observe(hitch);
  pacing.observe(hitch);
  const auto report = pacing.report("high");
  EXPECT_FALSE(report["passed"].toBool());
  EXPECT_EQ(report["hitch_frames"].toInt(), 2);
  const auto clusters = report["clusters"].toArray();
  ASSERT_EQ(clusters.size(), 1);
  EXPECT_EQ(clusters[0].toObject()["frames"].toInt(), 2);
  EXPECT_EQ(clusters[0]
                .toObject()["worst_frame_evidence"]
                .toObject()["largest_cpu_phase"]
                .toString(),
            "submit");
}

TEST(FramePacingTest, MissingMeasurementsAndInvalidValuesFailClosed) {
  Render::Profiling::FramePacing pacing;
  EXPECT_FALSE(pacing.report("high")["passed"].toBool());
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 0, 0, {}});
  }
  EXPECT_FALSE(pacing.report("high")["passed"].toBool());
  pacing.reset();
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 8, 0, {}});
  }
  EXPECT_FALSE(pacing.report("unknown")["passed"].toBool());
  pacing.observe({std::numeric_limits<double>::quiet_NaN(), 8, 8, 0, {}});
  EXPECT_FALSE(pacing.report("high")["passed"].toBool());
}

TEST(FramePacingTest, UploadBudgetsDependOnPresetAndResetClearsHitches) {
  Render::Profiling::FramePacing pacing;
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 8, 3 * 1024 * 1024, {}});
  }
  EXPECT_FALSE(pacing.report("low")["passed"].toBool());
  EXPECT_TRUE(pacing.report("medium")["passed"].toBool());
  pacing.observe({100, 90, 8, 0, {}});
  EXPECT_FALSE(pacing.report("medium")["passed"].toBool());
  pacing.reset();
  EXPECT_EQ(pacing.report("medium")["hitch_frames"].toInt(), 0);
}

TEST(FramePacingTest, PresentationInputCycleRepeatsAndDoesNotDependOnFrameCount) {
  using Render::Profiling::presentation_cycle_position;
  const auto start = presentation_cycle_position(0);
  const auto end = presentation_cycle_position(20);
  EXPECT_NEAR(start.x, end.x, 1e-9);
  EXPECT_NEAR(start.z, end.z, 1e-9);
  EXPECT_NEAR(start.zoom, end.zoom, 1e-9);
  const auto first = presentation_cycle_position(7.25);
  const auto repeat = presentation_cycle_position(27.25);
  EXPECT_NEAR(first.x, repeat.x, 1e-9);
  EXPECT_NEAR(first.z, repeat.z, 1e-9);
  EXPECT_NEAR(first.zoom, repeat.zoom, 1e-9);
}

TEST(FramePacingTest, AssetWorkFailsEvenWhenFrameTimeIsWithinBudget) {
  Render::Profiling::FramePacing pacing;
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 8, 0, {}});
  }
  Render::Profiling::PacingSample sample{16.67, 8, 8, 0, {}};
  sample.asset_work = 1;
  pacing.observe(sample);
  const auto report = pacing.report("high");
  EXPECT_FALSE(report["passed"].toBool());
  EXPECT_DOUBLE_EQ(report["checks"]
                       .toObject()["post_playable_asset_work"]
                       .toObject()["measured"]
                       .toDouble(),
                   1);
}

TEST(FramePacingTest, OneGpuSampleCannotCertifyAnOtherwiseUntimedRun) {
  Render::Profiling::FramePacing pacing;
  for (int i = 0; i < 1800; ++i) {
    pacing.observe({16.67, 8, 0, 0, {}});
  }
  pacing.observe({16.67, 8, 8, 0, {}});
  const auto report = pacing.report("high");
  EXPECT_FALSE(report["passed"].toBool());
  EXPECT_FALSE(report["checks"]
                   .toObject()["missing_gpu_sample_fraction"]
                   .toObject()["passed"]
                   .toBool());
}
