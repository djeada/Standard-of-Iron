#include "gl_creation_trace.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "asset_counters.h"

#if defined(__linux__)
#include <cxxabi.h>
#include <execinfo.h>
#define SOI_HAS_BACKTRACE 1
#else
#define SOI_HAS_BACKTRACE 0
#endif

namespace Render::Profiling {

namespace {

constexpr int k_max_frames = 24;
constexpr int k_skipped_frames = 2;

struct SiteRecord {
  std::string kind;
  std::vector<void*> frames;
  std::size_t count = 0;
};

struct TraceState {
  std::mutex mutex;
  std::map<std::string, SiteRecord> sites;
  std::map<std::string, std::size_t> mesh_uploads;
};

auto trace_state() -> TraceState& {
  static TraceState state;
  return state;
}

auto playable_window_flag() noexcept -> std::atomic_bool& {
  static std::atomic_bool open{false};
  return open;
}

auto key_for(std::string_view kind,
             const void* const* frames,
             int count) -> std::string {
  std::string key(kind);
  key.reserve(key.size() + static_cast<std::size_t>(count) * 17U);
  std::array<char, 20> digits{};
  for (int index = 0; index < count; ++index) {
    const int written =
        std::snprintf(digits.data(),
                      digits.size(),
                      "|%llx",
                      static_cast<unsigned long long>(
                          reinterpret_cast<std::uintptr_t>(frames[index])));
    key.append(digits.data(), static_cast<std::size_t>(std::max(written, 0)));
  }
  return key;
}

auto demangled(const std::string& symbol) -> std::string {
  const auto open = symbol.find('(');
  const auto plus = symbol.find('+', open == std::string::npos ? 0 : open);
  if (open == std::string::npos || plus == std::string::npos || plus <= open + 1) {
    return symbol;
  }
  const std::string mangled = symbol.substr(open + 1, plus - open - 1);
#if SOI_HAS_BACKTRACE
  int status = 0;
  char* pretty = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
  if (status == 0 && pretty != nullptr) {
    std::string result(pretty);
    std::free(pretty);
    return result;
  }
#endif
  return mangled.empty() ? symbol : mangled;
}

} // namespace

auto gl_creation_trace_enabled() noexcept -> bool {
#if SOI_HAS_BACKTRACE
  static const bool enabled = [] {
    const char* value = std::getenv("SOI_TRACE_PLAYABLE_GL");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
  }();
  return enabled;
#else
  return false;
#endif
}

void set_playable_window_open(bool open) noexcept {
  playable_window_flag().store(open, std::memory_order_release);
}

auto playable_window_open() noexcept -> bool {
  return playable_window_flag().load(std::memory_order_acquire);
}

void record_gl_creation(std::string_view kind, std::size_t count) noexcept {
#if SOI_HAS_BACKTRACE
  if (count == 0 || !gl_creation_trace_enabled()) {
    return;
  }
  if (!playable_window_open()) {
    return;
  }
  std::array<void*, k_max_frames> frames{};
  const int captured = backtrace(frames.data(), k_max_frames);
  if (captured <= k_skipped_frames) {
    return;
  }
  const int useful = captured - k_skipped_frames;
  const std::string key = key_for(kind, frames.data() + k_skipped_frames, useful);
  auto& state = trace_state();
  const std::lock_guard<std::mutex> guard(state.mutex);
  auto [entry, inserted] = state.sites.try_emplace(key);
  if (inserted) {
    entry->second.kind = std::string(kind);
    entry->second.frames.assign(frames.begin() + k_skipped_frames,
                                frames.begin() + captured);
  }
  entry->second.count += count;
#else
  (void)kind;
  (void)count;
#endif
}

void record_mesh_upload(const std::string& fingerprint) noexcept {
  if (!gl_creation_trace_enabled()) {
    return;
  }
  auto& state = trace_state();
  const std::lock_guard<std::mutex> guard(state.mutex);
  ++state.mesh_uploads[fingerprint];
}

auto mesh_upload_report(std::size_t max_entries) -> QJsonArray {
  QJsonArray report;
  auto& state = trace_state();
  const std::lock_guard<std::mutex> guard(state.mutex);
  std::vector<const std::pair<const std::string, std::size_t>*> ordered;
  ordered.reserve(state.mesh_uploads.size());
  for (const auto& entry : state.mesh_uploads) {
    ordered.push_back(&entry);
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
    return left->second > right->second;
  });
  for (const auto* entry : ordered) {
    if (static_cast<std::size_t>(report.size()) >= max_entries) {
      break;
    }
    report.append(
        QJsonObject{{QStringLiteral("mesh"), QString::fromStdString(entry->first)},
                    {QStringLiteral("uploads"), static_cast<qint64>(entry->second)}});
  }
  return report;
}

void reset_gl_creation_trace() noexcept {
  auto& state = trace_state();
  const std::lock_guard<std::mutex> guard(state.mutex);
  state.sites.clear();
  state.mesh_uploads.clear();
}

auto gl_creation_trace_report(std::size_t max_sites) -> QJsonArray {
  QJsonArray report;
#if SOI_HAS_BACKTRACE
  auto& state = trace_state();
  const std::lock_guard<std::mutex> guard(state.mutex);
  std::vector<const SiteRecord*> ordered;
  ordered.reserve(state.sites.size());
  for (const auto& [key, record] : state.sites) {
    ordered.push_back(&record);
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
    return left->count > right->count;
  });
  for (const auto* record : ordered) {
    if (static_cast<std::size_t>(report.size()) >= max_sites) {
      break;
    }
    char** symbols = backtrace_symbols(record->frames.data(),
                                       static_cast<int>(record->frames.size()));
    QJsonArray stack;
    if (symbols != nullptr) {
      for (std::size_t index = 0; index < record->frames.size(); ++index) {
        stack.append(QString::fromStdString(demangled(std::string(symbols[index]))));
      }
      std::free(symbols);
    }
    report.append(
        QJsonObject{{QStringLiteral("kind"), QString::fromStdString(record->kind)},
                    {QStringLiteral("count"), static_cast<qint64>(record->count)},
                    {QStringLiteral("stack"), stack}});
  }
#else
  (void)max_sites;
#endif
  return report;
}

} // namespace Render::Profiling
