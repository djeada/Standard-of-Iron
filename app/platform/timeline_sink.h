#pragma once

#include <memory>
#include <string>

namespace App::Platform {

// One marker on the platform's session timeline (Steam Timeline). Titles and
// descriptions are already player-facing text; the sink never builds them.
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

// The sink used when no platform is present. Gameplay never checks for it.
class NullTimelineSink final : public TimelineSink {
public:
  void add_marker(const TimelineMarker&) override {}
};

// The Steam-backed sink, or the null sink when the build has no Steamworks SDK
// or the Steam client is not running.
auto make_platform_timeline_sink() -> std::unique_ptr<TimelineSink>;

} // namespace App::Platform
