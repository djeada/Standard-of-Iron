#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Audio {

enum class CueOutcome : std::uint8_t {
  Accepted,
  Unbound,
  CueCooldown,
  NoLoadedResource,
  SystemStopped,
  InstanceLimit,
  ResourceCooldown,
  GlobalPriority,
  CategoryPriority,
  Muted,
  ResourceNotLoaded,
  AudienceFiltered,
};

inline constexpr std::size_t k_cue_outcome_count = 12;

auto cue_outcome_name(CueOutcome outcome) -> const char*;

auto cue_source(const char* file, unsigned line) -> std::string;

inline auto cue_source_of(const std::source_location& location =
                              std::source_location::current()) -> std::string {
  return cue_source(location.file_name(), location.line());
}

struct ListenerContext {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  std::string mode{"none"};
};

struct CueRequestSnapshot {
  std::string cue_id;
  std::string resource_id;
  std::string source;
  CueOutcome outcome{CueOutcome::Accepted};
  double seconds{0.0};
};

struct CueTraceRecord {
  std::string cue_id;
  std::uint64_t requests{0};
  std::uint64_t accepted{0};
  std::array<std::uint64_t, k_cue_outcome_count> outcomes{};
  std::vector<std::string> sources;
  std::map<std::string, std::uint64_t> resource_plays;
  std::string last_resource_id;
  double last_request_seconds{0.0};
};

class CueTrace {
public:
  static auto instance() -> CueTrace&;

  static auto logging_enabled() -> bool;
  static void set_logging_enabled(bool enabled);

  void record(const std::string& cue_id,
              const std::string& resource_id,
              CueOutcome outcome,
              const std::string& source = {});

  void set_listener(const ListenerContext& listener);
  [[nodiscard]] auto listener() const -> ListenerContext;

  void reset();

  [[nodiscard]] auto records() const -> std::vector<CueTraceRecord>;
  [[nodiscard]] auto last_request() const -> CueRequestSnapshot;
  [[nodiscard]] auto
  struggling_cues(std::uint64_t min_requests = 3U) const -> std::vector<CueTraceRecord>;
  [[nodiscard]] auto record_for(const std::string& cue_id) const -> CueTraceRecord;
  [[nodiscard]] auto requested_cues() const -> std::vector<std::string>;
  [[nodiscard]] auto never_accepted_cues() const -> std::vector<std::string>;

  auto write_summary(const std::string& path,
                     const std::string& label = {}) const -> bool;
  auto write_requested_summary(const std::string& label = {}) -> bool;

private:
  CueTrace();

  CueTrace(const CueTrace&) = delete;
  auto operator=(const CueTrace&) -> CueTrace& = delete;

  [[nodiscard]] auto elapsed_seconds_locked() const -> double;

  mutable std::mutex m_mutex;
  std::unordered_map<std::string, CueTraceRecord> m_records;
  ListenerContext m_listener;
  CueRequestSnapshot m_last_request;
  std::chrono::steady_clock::time_point m_started_at;
  unsigned m_exports{0};
};

auto format_status_overlay() -> std::string;

} // namespace Game::Audio
