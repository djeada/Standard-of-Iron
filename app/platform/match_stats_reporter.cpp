#include "match_stats_reporter.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace App::Platform {

MatchStatsReporter::MatchStatsReporter(std::shared_ptr<StatsSink> sink)
    : m_sink(std::move(sink)) {
  if (!m_sink) {
    m_sink = std::make_shared<NullStatsSink>();
  }
}

void MatchStatsReporter::add_total(const char* name, int delta) {
  if (delta <= 0) {
    return;
  }
  int current = 0;
  if (!m_sink->get_int(name, current)) {
    return;
  }
  const long long sum = static_cast<long long>(current) + delta;
  m_sink->set_int(
      name,
      static_cast<int>(std::min<long long>(sum, std::numeric_limits<int>::max())));
}

void MatchStatsReporter::raise_maximum(const char* name, int value) {
  int current = 0;
  if (!m_sink->get_int(name, current)) {
    return;
  }
  if (value > current) {
    m_sink->set_int(name, value);
  }
}

void MatchStatsReporter::report(const MatchSummary& summary) {
  if (m_reported) {
    return;
  }
  m_reported = true;
  if (summary.victory) {
    add_total("soi_battles_won", 1);
    if (summary.campaign_mission) {
      add_total("soi_missions_completed", 1);
    }
  }
  add_total("soi_enemies_defeated", summary.enemies_defeated);
  add_total("soi_units_recruited", summary.units_recruited);
  add_total("soi_waves_cleared", summary.waves_cleared);
  raise_maximum("soi_best_match_kills", summary.enemies_defeated);
  raise_maximum("soi_highest_army_size", m_peak_army);
  m_sink->store();
}

void MatchStatsReporter::note_army_size(int men) {
  m_peak_army = std::max(m_peak_army, men);
}

void MatchStatsReporter::match_ended() {
  m_reported = false;
  m_peak_army = 0;
}

} // namespace App::Platform
