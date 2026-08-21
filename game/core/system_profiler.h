#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Engine::Core {

class SystemProfiler {
public:
  struct SystemRecord {
    std::string name;
    std::uint64_t last_us{0};
    std::uint64_t total_us{0};
    std::uint64_t peak_us{0};
    std::uint64_t calls{0};

    std::uint64_t last_views{0};
    std::uint64_t last_view_candidates{0};
    std::uint64_t last_collects{0};
    std::uint64_t last_collected_entities{0};
    std::uint64_t last_spatial_queries{0};
    std::uint64_t last_spatial_candidates{0};

    [[nodiscard]] auto average_us() const -> double {
      return calls == 0 ? 0.0
                        : static_cast<double>(total_us) / static_cast<double>(calls);
    }
  };

  struct QueryCounters {
    std::uint64_t views{0};
    std::uint64_t view_candidates{0};
    std::uint64_t collects{0};
    std::uint64_t collected_entities{0};
    std::uint64_t spatial_queries{0};
    std::uint64_t spatial_candidates{0};

    auto operator-(const QueryCounters& other) const -> QueryCounters {
      return QueryCounters{
          .views = views - other.views,
          .view_candidates = view_candidates - other.view_candidates,
          .collects = collects - other.collects,
          .collected_entities = collected_entities - other.collected_entities,
          .spatial_queries = spatial_queries - other.spatial_queries,
          .spatial_candidates = spatial_candidates - other.spatial_candidates};
    }
  };

  struct CallSite {
    std::uint64_t calls{0};
    std::uint64_t entities{0};
  };

  struct TickSummary {
    std::uint64_t tick_index{0};
    std::uint64_t total_us{0};
    std::size_t entity_count{0};
    QueryCounters queries;
  };

  void set_enabled(bool enabled) noexcept { m_enabled = enabled; }
  [[nodiscard]] auto enabled() const noexcept -> bool { return m_enabled; }

  void begin_tick(std::uint64_t tick_index, std::size_t entity_count);
  void record_system(std::size_t slot,
                     const char* name,
                     std::uint64_t elapsed_us,
                     const QueryCounters& delta);
  void end_tick(std::uint64_t total_us);

  [[nodiscard]] auto systems() const -> const std::vector<SystemRecord>& {
    return m_systems;
  }
  [[nodiscard]] auto last_tick() const -> const TickSummary& { return m_last_tick; }
  [[nodiscard]] auto ticks_recorded() const -> std::uint64_t { return m_ticks; }

  void note_collect_call_site(const char* file, unsigned line, std::size_t entities);

  [[nodiscard]] auto
  collect_call_sites() const -> const std::map<std::string, CallSite>& {
    return m_collect_call_sites;
  }

  void clear();

  [[nodiscard]] auto format_report() const -> std::string;

private:
  bool m_enabled{false};
  std::uint64_t m_ticks{0};
  std::vector<SystemRecord> m_systems;
  std::map<std::string, CallSite> m_collect_call_sites;
  TickSummary m_last_tick;
};

} // namespace Engine::Core
