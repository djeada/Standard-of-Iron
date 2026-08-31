#include "system_profiler.h"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <utility>

namespace Engine::Core {

void SystemProfiler::begin_tick(std::uint64_t tick_index, std::size_t entity_count) {
  if (!m_enabled) {
    return;
  }
  m_last_tick.tick_index = tick_index;
  m_last_tick.entity_count = entity_count;
  m_last_tick.queries = {};
  m_last_tick.total_us = 0;
}

void SystemProfiler::record_system(std::size_t slot,
                                   const char* name,
                                   std::uint64_t elapsed_us,
                                   const QueryCounters& delta) {
  if (!m_enabled) {
    return;
  }
  if (slot >= m_systems.size()) {
    m_systems.resize(slot + 1U);
  }
  SystemRecord& record = m_systems[slot];
  if (record.name.empty() && name != nullptr) {
    record.name = name;
  }
  record.last_us = elapsed_us;
  record.total_us += elapsed_us;
  record.peak_us = std::max(record.peak_us, elapsed_us);
  record.samples.push(static_cast<double>(elapsed_us));
  ++record.calls;

  record.last_views = delta.views;
  record.last_view_candidates = delta.view_candidates;
  record.last_collects = delta.collects;
  record.last_collected_entities = delta.collected_entities;
  record.last_spatial_queries = delta.spatial_queries;
  record.last_spatial_candidates = delta.spatial_candidates;

  m_last_tick.queries.views += delta.views;
  m_last_tick.queries.view_candidates += delta.view_candidates;
  m_last_tick.queries.collects += delta.collects;
  m_last_tick.queries.collected_entities += delta.collected_entities;
  m_last_tick.queries.spatial_queries += delta.spatial_queries;
  m_last_tick.queries.spatial_candidates += delta.spatial_candidates;
}

void SystemProfiler::end_tick(std::uint64_t total_us) {
  if (!m_enabled) {
    return;
  }
  m_last_tick.total_us = total_us;
  m_tick_us.push(static_cast<double>(total_us));
  ++m_ticks;
}

void SystemProfiler::note_collect_call_site(const char* file,
                                            unsigned line,
                                            std::size_t entities) {
  if (!m_enabled || file == nullptr) {
    return;
  }

  std::string_view path(file);
  const std::size_t slash = path.find_last_of('/');
  if (slash != std::string_view::npos) {
    path.remove_prefix(slash + 1U);
  }

  std::string key(path);
  key += ':';
  key += std::to_string(line);

  CallSite& site = m_collect_call_sites[key];
  ++site.calls;
  site.entities += entities;
}

void SystemProfiler::clear() {
  m_systems.clear();
  m_collect_call_sites.clear();
  m_last_tick = {};
  m_tick_us.clear();
  m_ticks = 0;
}

auto SystemProfiler::format_report() const -> std::string {
  std::vector<const SystemRecord*> ordered;
  ordered.reserve(m_systems.size());
  for (const SystemRecord& record : m_systems) {
    if (record.calls > 0) {
      ordered.push_back(&record);
    }
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto* lhs, const auto* rhs) {
    return lhs->average_us() > rhs->average_us();
  });

  std::string out;
  char line[256];

  std::snprintf(line,
                sizeof(line),
                "%-30s %9s %9s %9s %9s %9s %8s %9s\n",
                "system",
                "avg us",
                "p50 us",
                "p95 us",
                "p99 us",
                "peak us",
                "queries",
                "collected");
  out += line;
  out += std::string(100, '-');
  out += '\n';

  for (const SystemRecord* record : ordered) {
    const Utils::Stats::Distribution spread = record->distribution_us();
    std::snprintf(line,
                  sizeof(line),
                  "%-30s %9.1f %9.1f %9.1f %9.1f %9llu %8llu %9llu\n",
                  record->name.c_str(),
                  record->average_us(),
                  spread.p50,
                  spread.p95,
                  spread.p99,
                  static_cast<unsigned long long>(record->peak_us),
                  static_cast<unsigned long long>(record->last_views +
                                                  record->last_collects +
                                                  record->last_spatial_queries),
                  static_cast<unsigned long long>(record->last_collected_entities));
    out += line;
  }

  if (!m_collect_call_sites.empty()) {
    std::vector<std::pair<const std::string*, const CallSite*>> sites;
    sites.reserve(m_collect_call_sites.size());
    for (const auto& [where, site] : m_collect_call_sites) {
      sites.emplace_back(&where, &site);
    }
    std::sort(sites.begin(), sites.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.second->entities > rhs.second->entities;
    });

    out += "\nmaterialising queries by call site (whole run, worst first)\n";
    const std::size_t shown = std::min<std::size_t>(sites.size(), 12U);
    for (std::size_t i = 0; i < shown; ++i) {
      std::snprintf(line,
                    sizeof(line),
                    "  %-44s %10llu calls %14llu entities\n",
                    sites[i].first->c_str(),
                    static_cast<unsigned long long>(sites[i].second->calls),
                    static_cast<unsigned long long>(sites[i].second->entities));
      out += line;
    }
  }

  const QueryCounters& queries = m_last_tick.queries;
  const double spatial_efficiency =
      queries.spatial_queries == 0 ? 0.0
                                   : static_cast<double>(queries.spatial_candidates) /
                                         static_cast<double>(queries.spatial_queries);
  const Utils::Stats::Distribution tick_spread = tick_time_us();
  std::snprintf(line,
                sizeof(line),
                "\ntick total us: avg %.1f  p50 %.1f  p95 %.1f  p99 %.1f  max %.1f\n",
                tick_spread.average,
                tick_spread.p50,
                tick_spread.p95,
                tick_spread.p99,
                tick_spread.maximum);
  out += line;

  std::snprintf(line,
                sizeof(line),
                "\nlast tick %llu: %llu entities, %llu us total\n"
                "  views %llu (%llu candidates)  collects %llu (%llu entities)\n"
                "  spatial queries %llu (%llu candidates, %.1f per query)\n",
                static_cast<unsigned long long>(m_last_tick.tick_index),
                static_cast<unsigned long long>(m_last_tick.entity_count),
                static_cast<unsigned long long>(m_last_tick.total_us),
                static_cast<unsigned long long>(queries.views),
                static_cast<unsigned long long>(queries.view_candidates),
                static_cast<unsigned long long>(queries.collects),
                static_cast<unsigned long long>(queries.collected_entities),
                static_cast<unsigned long long>(queries.spatial_queries),
                static_cast<unsigned long long>(queries.spatial_candidates),
                spatial_efficiency);
  out += line;

  return out;
}

} // namespace Engine::Core
