#pragma once

#include <memory>

#include "stats_sink.h"

namespace App::Platform {

// What one finished match contributes, taken from the authoritative per-owner
// counters, not from presentation data.
struct MatchSummary {
  bool victory = false;
  bool campaign_mission = false;
  int enemies_defeated = 0;
  int units_recruited = 0;
  int waves_cleared = 0;
};

// Folds finished matches into the platform's lifetime stats.
//   totals   soi_battles_won, soi_missions_completed, soi_enemies_defeated,
//            soi_units_recruited, soi_waves_cleared: the platform value plus
//            this match's share.
//   maxima   soi_best_match_kills: the larger of the platform value and this
//            match. soi_highest_army_size likewise, from note_army_size().
// One store() per match, never one call per soldier. A match is folded once
// even if its outcome is reported again; match_ended() re-arms the reporter.
class MatchStatsReporter {
public:
  explicit MatchStatsReporter(std::shared_ptr<StatsSink> sink);

  // Sampled by the engine as the player's army changes; only the peak counts.
  void note_army_size(int men);
  void report(const MatchSummary& summary);
  void match_ended();

private:
  void add_total(const char* name, int delta);
  void raise_maximum(const char* name, int value);

  std::shared_ptr<StatsSink> m_sink;
  bool m_reported = false;
  int m_peak_army = 0;
};

} // namespace App::Platform
