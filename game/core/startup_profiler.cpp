#include "core/startup_profiler.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <sstream>
#include <utility>

namespace Engine::Core {
namespace {

constexpr std::size_t k_max_frame_samples = 2048;
constexpr double k_first_window_ms = 1000.0;
constexpr double k_second_window_ms = 5000.0;

} // namespace

auto StartupProfiler::instance() -> StartupProfiler& {
  static StartupProfiler profiler;
  return profiler;
}

auto StartupProfiler::tracing_enabled() -> bool {
  static const bool enabled = qEnvironmentVariableIsSet("SOI_STARTUP_TRACE");
  return enabled;
}

auto StartupProfiler::reporting_enabled() -> bool {
  return tracing_enabled() || !trace_file_path().empty();
}

void StartupProfiler::begin_run(std::string label) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_active = true;
  m_label = std::move(label);
  m_run_started = std::chrono::steady_clock::now();
  m_phases.clear();
  m_counters.clear();
  m_overlay_released_ms = -1.0;
  m_first_playable_frame_ms = -1.0;
  m_frames.clear();
}

auto StartupProfiler::elapsed_ms_locked() const -> double {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                   m_run_started)
      .count();
}

void StartupProfiler::record_phase(std::string_view name,
                                   double duration_ms,
                                   std::int64_t items) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_active) {
    return;
  }
  const auto found =
      std::find_if(m_phases.begin(), m_phases.end(), [&name](const PhaseRecord& phase) {
        return phase.name == name;
      });
  if (found != m_phases.end()) {
    found->duration_ms += duration_ms;
    found->items += items;
    ++found->entries;
    return;
  }
  m_phases.push_back({.name = std::string(name),
                      .duration_ms = duration_ms,
                      .entries = 1,
                      .items = items});
}

void StartupProfiler::add_counter(std::string_view name, std::int64_t value) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_active) {
    return;
  }
  const auto found =
      std::find_if(m_counters.begin(),
                   m_counters.end(),
                   [&name](const Counter& counter) { return counter.name == name; });
  if (found != m_counters.end()) {
    found->value += value;
    return;
  }
  m_counters.push_back({.name = std::string(name), .value = value});
}

void StartupProfiler::mark_overlay_released() {
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_active || m_overlay_released_ms >= 0.0) {
    return;
  }
  m_overlay_released_ms = elapsed_ms_locked();
}

void StartupProfiler::record_playable_frame(double frame_duration_ms) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_active || m_overlay_released_ms < 0.0) {
    return;
  }
  const double now_ms = elapsed_ms_locked();
  if (m_first_playable_frame_ms < 0.0) {
    m_first_playable_frame_ms = now_ms;
  }
  if (m_frames.size() >= k_max_frame_samples) {
    return;
  }
  m_frames.push_back({.offset_ms = now_ms - m_first_playable_frame_ms,
                      .duration_ms = frame_duration_ms});
}

auto StartupProfiler::active() const -> bool {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_active;
}

auto StartupProfiler::report() const -> Report {
  const std::lock_guard<std::mutex> lock(m_mutex);

  Report out;
  out.label = m_label;
  out.phases = m_phases;
  out.counters = m_counters;
  out.overlay_released_ms = m_overlay_released_ms;
  out.first_playable_frame_ms = m_first_playable_frame_ms;

  std::vector<double> first_second;
  std::vector<double> first_five;
  for (const auto& sample : m_frames) {
    if (sample.offset_ms <= k_first_window_ms) {
      first_second.push_back(sample.duration_ms);
    }
    if (sample.offset_ms <= k_second_window_ms) {
      first_five.push_back(sample.duration_ms);
    }
  }
  if (!m_frames.empty()) {
    out.first_frame_duration_ms = m_frames.front().duration_ms;
  }
  out.first_second.window_seconds = k_first_window_ms / 1000.0;
  out.first_second.distribution =
      Utils::Stats::distribution_of(std::move(first_second));
  out.first_five_seconds.window_seconds = k_second_window_ms / 1000.0;
  out.first_five_seconds.distribution =
      Utils::Stats::distribution_of(std::move(first_five));
  return out;
}

auto StartupProfiler::format_report() const -> std::string {
  const Report snapshot = report();

  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(2);
  out << "SOI_STARTUP mission=" << snapshot.label << '\n';

  double total_ms = 0.0;
  for (const auto& phase : snapshot.phases) {
    total_ms += phase.duration_ms;
    out << "  phase " << phase.name << " = " << phase.duration_ms << " ms";
    if (phase.entries > 1) {
      out << " (x" << phase.entries << ')';
    }
    if (phase.items != 0) {
      out << " items=" << phase.items;
    }
    out << '\n';
  }
  out << "  phase total = " << total_ms << " ms\n";

  for (const auto& counter : snapshot.counters) {
    out << "  count " << counter.name << " = " << counter.value << '\n';
  }

  out << "  overlay released at = " << snapshot.overlay_released_ms << " ms\n";
  out << "  first playable frame at = " << snapshot.first_playable_frame_ms << " ms";
  if (snapshot.first_frame_duration_ms >= 0.0) {
    out << " (" << snapshot.first_frame_duration_ms << " ms)";
  }
  out << '\n';

  const auto window = [&out](const char* label, const FrameWindow& frames) {
    const auto& distribution = frames.distribution;
    out << "  frames " << label << " n=" << distribution.count
        << " avg=" << distribution.average << " p95=" << distribution.p95
        << " p99=" << distribution.p99 << " worst=" << distribution.maximum << '\n';
  };
  window("first 1s", snapshot.first_second);
  window("first 5s", snapshot.first_five_seconds);

  return out.str();
}

auto StartupProfiler::report_as_json() const -> std::string {
  const Report snapshot = report();

  QJsonArray phases;
  for (const auto& phase : snapshot.phases) {
    phases.append(
        QJsonObject{{QStringLiteral("name"), QString::fromStdString(phase.name)},
                    {QStringLiteral("duration_ms"), phase.duration_ms},
                    {QStringLiteral("entries"), static_cast<qint64>(phase.entries)},
                    {QStringLiteral("items"), static_cast<qint64>(phase.items)}});
  }

  QJsonObject counters;
  for (const auto& counter : snapshot.counters) {
    counters.insert(QString::fromStdString(counter.name),
                    static_cast<qint64>(counter.value));
  }

  const auto window = [](const FrameWindow& frames) {
    const auto& distribution = frames.distribution;
    return QJsonObject{
        {QStringLiteral("window_seconds"), frames.window_seconds},
        {QStringLiteral("count"), static_cast<qint64>(distribution.count)},
        {QStringLiteral("average_ms"), distribution.average},
        {QStringLiteral("p50_ms"), distribution.p50},
        {QStringLiteral("p95_ms"), distribution.p95},
        {QStringLiteral("p99_ms"), distribution.p99},
        {QStringLiteral("worst_ms"), distribution.maximum}};
  };

  const QJsonObject root{
      {QStringLiteral("mission"), QString::fromStdString(snapshot.label)},
      {QStringLiteral("phases"), phases},
      {QStringLiteral("counters"), counters},
      {QStringLiteral("overlay_released_ms"), snapshot.overlay_released_ms},
      {QStringLiteral("first_playable_frame_ms"), snapshot.first_playable_frame_ms},
      {QStringLiteral("first_frame_duration_ms"), snapshot.first_frame_duration_ms},
      {QStringLiteral("first_second"), window(snapshot.first_second)},
      {QStringLiteral("first_five_seconds"), window(snapshot.first_five_seconds)}};

  return QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString();
}

auto StartupProfiler::trace_file_path() -> std::string {
  static const std::string path =
      qEnvironmentVariable("SOI_STARTUP_TRACE_FILE").toStdString();
  return path;
}

auto StartupProfiler::write_report(const std::string& path) const -> bool {
  if (path.empty()) {
    return false;
  }
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "StartupProfiler: cannot write" << file.fileName() << ":"
               << file.errorString();
    return false;
  }
  const std::string payload = report_as_json();
  file.write(payload.data(), static_cast<qint64>(payload.size()));
  return true;
}

void StartupProfiler::log_report() const {
  if (tracing_enabled()) {
    qInfo().noquote() << QString::fromStdString(format_report());
  }
  write_report(trace_file_path());
}

ScopedStartupPhase::ScopedStartupPhase(std::string_view name)
    : m_name(name)
    , m_started(std::chrono::steady_clock::now()) {
}

ScopedStartupPhase::~ScopedStartupPhase() {
  const double elapsed_ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - m_started)
                                .count();
  StartupProfiler::instance().record_phase(m_name, elapsed_ms, m_items);
}

} // namespace Engine::Core
