#include "timeline_sink.h"

#ifdef SOI_STEAMWORKS
#include <steam/steam_api.h>
#endif

namespace App::Platform {

#ifdef SOI_STEAMWORKS
namespace {

class SteamTimelineSink final : public TimelineSink {
public:
  void add_marker(const TimelineMarker& marker) override {
    ISteamTimeline* timeline = SteamTimeline();
    if (timeline == nullptr) {
      return;
    }
    timeline->AddInstantaneousTimelineEvent(marker.title.c_str(),
                                            marker.description.c_str(),
                                            marker.icon.c_str(),
                                            marker.priority,
                                            0.0F,
                                            k_ETimelineEventClipPriority_Standard);
  }
};

} // namespace
#endif

auto make_platform_timeline_sink() -> std::unique_ptr<TimelineSink> {
#ifdef SOI_STEAMWORKS

  if (SteamAPI_Init()) {
    return std::make_unique<SteamTimelineSink>();
  }
#endif
  return std::make_unique<NullTimelineSink>();
}

} // namespace App::Platform
