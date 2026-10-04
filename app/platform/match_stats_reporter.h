#pragma once

#include <memory>

#include "stats_sink.h"

namespace App::Platform {

struct MatchSummary {
  bool victory = false;
  bool campaign_mission = false;
  int enemies_defeated = 0;
  int units_recruited = 0;
  int waves_cleared = 0;
};

class MatchStatsReporter {
public:
  explicit MatchStatsReporter(std::shared_ptr<StatsSink> sink);

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
