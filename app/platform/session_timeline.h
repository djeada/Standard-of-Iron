#pragma once

#include <memory>
#include <string>

#include "timeline_sink.h"

namespace App::Platform {

class SessionTimeline {
public:
  explicit SessionTimeline(std::shared_ptr<TimelineSink> sink);

  void mission_started(const std::string& mission_id);
  void mission_completed(const std::string& mission_id);
  void mission_failed(const std::string& mission_id);
  void campaign_completed(const std::string& campaign_id);
  void skirmish_result(bool victory);

  void match_ended();

private:
  void emit_once(const std::string& key, const TimelineMarker& marker);

  std::shared_ptr<TimelineSink> m_sink;
  std::string m_last_key;
};

} // namespace App::Platform
