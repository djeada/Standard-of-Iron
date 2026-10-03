#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "app/platform/session_timeline.h"

namespace {

using App::Platform::SessionTimeline;
using App::Platform::TimelineMarker;
using App::Platform::TimelineSink;

class RecordingSink final : public TimelineSink {
public:
  void add_marker(const TimelineMarker& marker) override { markers.push_back(marker); }
  std::vector<TimelineMarker> markers;
};

TEST(SessionTimeline, MissionLifecycleEmitsOneMarkerPerEvent) {
  auto sink = std::make_shared<RecordingSink>();
  SessionTimeline timeline(sink);
  timeline.mission_started("m1");
  timeline.mission_completed("m1");
  ASSERT_EQ(sink->markers.size(), 2U);
  EXPECT_EQ(sink->markers[0].title, "Mission begins");
  EXPECT_EQ(sink->markers[1].title, "Mission victory");
  EXPECT_GT(sink->markers[1].priority, sink->markers[0].priority);
}

TEST(SessionTimeline, RepeatedEventDoesNotRepeatTheMarker) {
  auto sink = std::make_shared<RecordingSink>();
  SessionTimeline timeline(sink);
  timeline.mission_completed("m1");
  timeline.mission_completed("m1");
  EXPECT_EQ(sink->markers.size(), 1U);
}

TEST(SessionTimeline, NextMatchMayEmitTheSameMarkerAgain) {
  auto sink = std::make_shared<RecordingSink>();
  SessionTimeline timeline(sink);
  timeline.mission_failed("m1");
  timeline.match_ended();
  timeline.mission_failed("m1");
  EXPECT_EQ(sink->markers.size(), 2U);
}

TEST(SessionTimeline, SkirmishResultsAreDistinct) {
  auto sink = std::make_shared<RecordingSink>();
  SessionTimeline timeline(sink);
  timeline.skirmish_result(true);
  timeline.skirmish_result(false);
  ASSERT_EQ(sink->markers.size(), 2U);
  EXPECT_NE(sink->markers[0].title, sink->markers[1].title);
}

TEST(SessionTimeline, NullSinkIsSafe) {
  SessionTimeline timeline(nullptr);
  timeline.campaign_completed("c1");
  SUCCEED();
}

} // namespace
