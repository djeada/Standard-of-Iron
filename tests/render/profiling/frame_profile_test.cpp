// Tests for the Stage-14 FrameProfile + overlay formatter.

#include "render/profiling/frame_profile.h"

#include <gtest/gtest.h>
#include <thread>

using Render::Profiling::format_overlay;
using Render::Profiling::FrameProfile;
using Render::Profiling::Phase;
using Render::Profiling::PhaseScope;

TEST(FrameProfileTest, ResetZeroes) {
  FrameProfile p;
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
  {
    PhaseScope scope(&p, Phase::Submit);
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
    PhaseScope scope(&p, Phase::Sort);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(p.total_us(), 0U);
}

TEST(FrameProfileTest, TotalSumsAllPhases) {
  FrameProfile p;
  p.add_phase_us(Phase::Collection, 100);
  p.add_phase_us(Phase::Sort, 200);
  p.add_phase_us(Phase::Playback, 300);
  EXPECT_EQ(p.total_us(), 600U);
}

TEST(FrameProfileTest, FormatOverlayIncludesAllPhases) {
  FrameProfile p;
  p.frame_index = 42;
  p.add_phase_us(Phase::Collection, 1000);
  p.add_phase_us(Phase::Sort, 500);
  p.add_phase_us(Phase::Playback, 2500);
  p.draw_calls = 123;
  p.triangles = 4567;
  p.instances = 890;
  p.budget_headroom_ms = 5.5;

  std::string s = format_overlay(p);
  EXPECT_NE(s.find("frame #42"), std::string::npos);
  EXPECT_NE(s.find("collect"), std::string::npos);
  EXPECT_NE(s.find("sort"), std::string::npos);
  EXPECT_NE(s.find("play"), std::string::npos);
  EXPECT_NE(s.find("draws=123"), std::string::npos);
  EXPECT_NE(s.find("tris=4567"), std::string::npos);
  EXPECT_NE(s.find("inst=890"), std::string::npos);
}

TEST(FrameProfileTest, FormatOverlayHandlesZeroTotal) {
  FrameProfile p;
  std::string s = format_overlay(p);
  EXPECT_NE(s.find("frame #0"), std::string::npos);
  EXPECT_NE(s.find("total"), std::string::npos);
  EXPECT_NE(s.find("draws="), std::string::npos);
}

TEST(FrameProfileTest, GlobalProfileIsSingleton) {
  auto &a = Render::Profiling::global_profile();
  auto &b = Render::Profiling::global_profile();
  EXPECT_EQ(&a, &b);
  a.reset();
  a.add_phase_us(Phase::Sort, 10);
  EXPECT_EQ(b.phase_us[static_cast<std::size_t>(Phase::Sort)], 10U);
  a.reset();
}

TEST(FrameProfileTest, PhaseNameMatchesEnum) {
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Collection), "collect");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Playback), "play");
  EXPECT_STREQ(Render::Profiling::phase_name(Phase::Present), "present");
}
