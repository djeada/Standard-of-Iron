#include "session_timeline.h"

#include <utility>

namespace App::Platform {

namespace {

constexpr unsigned k_priority_start = 10;
constexpr unsigned k_priority_result = 50;
constexpr unsigned k_priority_milestone = 100;

} // namespace

SessionTimeline::SessionTimeline(std::shared_ptr<TimelineSink> sink)
    : m_sink(std::move(sink)) {
  if (!m_sink) {
    m_sink = std::make_shared<NullTimelineSink>();
  }
}

void SessionTimeline::emit_once(const std::string& key, const TimelineMarker& marker) {
  if (key == m_last_key) {
    return;
  }
  m_last_key = key;
  m_sink->add_marker(marker);
}

void SessionTimeline::mission_started(const std::string& mission_id) {
  emit_once("start:" + mission_id,
            {"Mission begins", mission_id, "steam_flag", k_priority_start});
}

void SessionTimeline::mission_completed(const std::string& mission_id) {
  emit_once("won:" + mission_id,
            {"Mission victory", mission_id, "steam_completed", k_priority_result});
}

void SessionTimeline::mission_failed(const std::string& mission_id) {
  emit_once("lost:" + mission_id,
            {"Mission lost", mission_id, "steam_x", k_priority_result});
}

void SessionTimeline::campaign_completed(const std::string& campaign_id) {
  emit_once(
      "campaign:" + campaign_id,
      {"Campaign complete", campaign_id, "steam_achievement", k_priority_milestone});
}

void SessionTimeline::skirmish_result(bool victory) {
  emit_once(victory ? "skirmish:won" : "skirmish:lost",
            {victory ? "Skirmish victory" : "Skirmish defeat",
             "",
             victory ? "steam_completed" : "steam_x",
             k_priority_result});
}

void SessionTimeline::match_ended() {
  m_last_key.clear();
}

} // namespace App::Platform
