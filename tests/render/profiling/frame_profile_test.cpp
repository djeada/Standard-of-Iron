

#include <gtest/gtest.h>
#include <thread>

#include "render/profiling/frame_profile.h"

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
