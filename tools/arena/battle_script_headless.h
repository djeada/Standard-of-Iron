#pragma once

#include <QString>

#include <cstdint>
#include <vector>

#include "arena_scenario.h"

namespace Arena::Headless {

struct Options {
  float fixed_step_seconds{1.0F / 30.0F};

  float duration_override{0.0F};

  float digest_interval_seconds{1.0F};

  float trace_interval_seconds{0.0F};
};

struct DigestSample {
  float time_seconds{0.0F};
  std::uint64_t digest{0};
};

struct Result {
  bool started{false};
  QString error;
  QString scenario_id;
  float elapsed_seconds{0.0F};
  int spawned_units{0};
  std::vector<ArenaScenarioEvent> events;
  std::vector<DigestSample> digests;
  std::uint64_t final_digest{0};
  ArenaScenarioReport report;
  double wall_seconds{0.0};
};

[[nodiscard]] auto run(const ArenaScenarioDefinition& definition,
                       const Options& options = {}) -> Result;

struct DeterminismReport {
  bool deterministic{false};
  int runs{0};
  float divergent_at{-1.0F};
  QString detail;
};

[[nodiscard]] auto compare(const Result& first,
                           const Result& second) -> DeterminismReport;

[[nodiscard]] auto timeline_json(const std::vector<Result>& results,
                                 const QString& id) -> QByteArray;

} // namespace Arena::Headless
