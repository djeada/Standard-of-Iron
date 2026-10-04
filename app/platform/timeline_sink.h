#pragma once

#include <memory>
#include <string>

namespace App::Platform {

struct TimelineMarker {
  std::string title;
  std::string description;
  std::string icon;
  unsigned priority = 0;
};

class TimelineSink {
public:
  virtual ~TimelineSink() = default;
  virtual void add_marker(const TimelineMarker& marker) = 0;
};

class NullTimelineSink final : public TimelineSink {
public:
  void add_marker(const TimelineMarker&) override {}
};

auto make_platform_timeline_sink() -> std::unique_ptr<TimelineSink>;

} // namespace App::Platform
