#include "cue_trace.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>
#include <qglobal.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <iomanip>
#include <mutex>
#include <source_location>
#include <sstream>
#include <string>
#include <vector>

#include "audio_system.h"

namespace Game::Audio {

namespace {

constexpr std::size_t k_max_sources_per_cue = 8;

auto summary_path_from_environment() -> QString {
  return qEnvironmentVariable("SOI_AUDIO_TRACE_SUMMARY");
}

auto indexed_path(const QString& path, unsigned index) -> QString {
  if (index == 0U) {
    return path;
  }

  const QFileInfo info(path);
  const QString suffix = info.completeSuffix();
  const QString stem = info.completeBaseName();
  const QString directory = info.path();
  const QString name =
      suffix.isEmpty()
          ? QStringLiteral("%1.%2").arg(stem).arg(index + 1U)
          : QStringLiteral("%1.%2.%3").arg(stem).arg(index + 1U).arg(suffix);
  return directory.isEmpty() ? name : directory + QLatin1Char('/') + name;
}

auto logging_flag() -> std::atomic<bool>& {
  static std::atomic<bool> enabled{!qEnvironmentVariableIsEmpty("SOI_AUDIO_TRACE")};
  return enabled;
}

auto source_root() -> const std::string& {
  static const std::string root = [] {
    const std::string self = std::source_location::current().file_name();
    const std::string own_path = "game/audio/cue_trace.cpp";
    const auto at = self.rfind(own_path);
    return at == std::string::npos ? std::string{} : self.substr(0, at);
  }();
  return root;
}

} // namespace

auto cue_source(const char* file, unsigned line) -> std::string {
  if (file == nullptr) {
    return {};
  }

  std::string path(file);
  const std::string& root = source_root();
  if (!root.empty() && path.rfind(root, 0) == 0) {
    path.erase(0, root.size());
  }
  return path + ":" + std::to_string(line);
}

auto cue_outcome_name(CueOutcome outcome) -> const char* {
  switch (outcome) {
  case CueOutcome::Accepted:
    return "accepted";
  case CueOutcome::Unbound:
    return "unbound";
  case CueOutcome::CueCooldown:
    return "cue_cooldown";
  case CueOutcome::NoLoadedResource:
    return "no_loaded_resource";
  case CueOutcome::SystemStopped:
    return "audio_system_stopped";
  case CueOutcome::InstanceLimit:
    return "instance_limit";
  case CueOutcome::ResourceCooldown:
    return "resource_cooldown";
  case CueOutcome::GlobalPriority:
    return "global_priority";
  case CueOutcome::CategoryPriority:
    return "category_priority";
  case CueOutcome::Muted:
    return "muted";
  case CueOutcome::ResourceNotLoaded:
    return "resource_not_loaded";
  case CueOutcome::AudienceFiltered:
    return "audience_filtered";
  }
  return "unknown";
}

CueTrace::CueTrace()
    : m_started_at(std::chrono::steady_clock::now()) {
}

auto CueTrace::instance() -> CueTrace& {
  static CueTrace trace;
  return trace;
}

auto CueTrace::logging_enabled() -> bool {
  return logging_flag().load(std::memory_order_relaxed);
}

void CueTrace::set_logging_enabled(bool enabled) {
  logging_flag().store(enabled, std::memory_order_relaxed);
}

auto CueTrace::elapsed_seconds_locked() const -> double {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(now - m_started_at).count();
}

void CueTrace::record(const std::string& cue_id,
                      const std::string& resource_id,
                      CueOutcome outcome,
                      const std::string& source) {
  if (cue_id.empty()) {
    return;
  }

  double seconds = 0.0;
  ListenerContext listener;
  {
    std::lock_guard<std::mutex> const lock(m_mutex);

    seconds = elapsed_seconds_locked();
    listener = m_listener;

    auto& entry = m_records[cue_id];
    entry.cue_id = cue_id;
    ++entry.requests;
    entry.outcomes.at(static_cast<std::size_t>(outcome)) += 1U;
    if (outcome == CueOutcome::Accepted) {
      ++entry.accepted;
      if (!resource_id.empty()) {
        ++entry.resource_plays[resource_id];
      }
    }
    if (!resource_id.empty()) {
      entry.last_resource_id = resource_id;
    }
    if (!source.empty() && entry.sources.size() < k_max_sources_per_cue &&
        std::find(entry.sources.begin(), entry.sources.end(), source) ==
            entry.sources.end()) {
      entry.sources.push_back(source);
      std::sort(entry.sources.begin(), entry.sources.end());
    }
    entry.last_request_seconds = seconds;

    m_last_request = {.cue_id = cue_id,
                      .resource_id = resource_id,
                      .source = source,
                      .outcome = outcome,
                      .seconds = seconds};
  }

  if (!logging_enabled()) {
    return;
  }

  qInfo().noquote() << QStringLiteral("audio cue [%1s %2 %3,%4,%5] %6 -> %7: %8 (%9)")
                           .arg(seconds, 0, 'f', 3)
                           .arg(QString::fromStdString(listener.mode))
                           .arg(listener.x, 0, 'f', 1)
                           .arg(listener.y, 0, 'f', 1)
                           .arg(listener.z, 0, 'f', 1)
                           .arg(QString::fromStdString(cue_id),
                                resource_id.empty()
                                    ? QStringLiteral("-")
                                    : QString::fromStdString(resource_id),
                                QString::fromLatin1(cue_outcome_name(outcome)),
                                source.empty() ? QStringLiteral("-")
                                               : QString::fromStdString(source));
}

void CueTrace::set_listener(const ListenerContext& listener) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_listener = listener;
}

auto CueTrace::listener() const -> ListenerContext {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_listener;
}

void CueTrace::reset() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_records.clear();
  m_started_at = std::chrono::steady_clock::now();
}

auto CueTrace::records() const -> std::vector<CueTraceRecord> {
  std::lock_guard<std::mutex> const lock(m_mutex);

  std::vector<CueTraceRecord> out;
  out.reserve(m_records.size());
  for (const auto& [cue_id, entry] : m_records) {
    out.push_back(entry);
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    return a.cue_id < b.cue_id;
  });
  return out;
}

auto CueTrace::last_request() const -> CueRequestSnapshot {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_last_request;
}

auto CueTrace::struggling_cues(std::uint64_t min_requests) const
    -> std::vector<CueTraceRecord> {
  std::vector<CueTraceRecord> struggling;
  for (const auto& entry : records()) {
    if (entry.requests < min_requests) {
      continue;
    }
    if (entry.accepted * 2U >= entry.requests) {
      continue;
    }
    struggling.push_back(entry);
  }

  std::sort(struggling.begin(), struggling.end(), [](const auto& a, const auto& b) {
    const double a_ratio =
        static_cast<double>(a.accepted) / static_cast<double>(a.requests);
    const double b_ratio =
        static_cast<double>(b.accepted) / static_cast<double>(b.requests);
    if (a_ratio != b_ratio) {
      return a_ratio < b_ratio;
    }
    return a.requests > b.requests;
  });
  return struggling;
}

auto CueTrace::record_for(const std::string& cue_id) const -> CueTraceRecord {
  std::lock_guard<std::mutex> const lock(m_mutex);

  auto it = m_records.find(cue_id);
  if (it == m_records.end()) {
    return {};
  }
  return it->second;
}

auto CueTrace::requested_cues() const -> std::vector<std::string> {
  std::vector<std::string> cue_ids;
  for (const auto& entry : records()) {
    cue_ids.push_back(entry.cue_id);
  }
  return cue_ids;
}

auto CueTrace::never_accepted_cues() const -> std::vector<std::string> {
  std::vector<std::string> cue_ids;
  for (const auto& entry : records()) {
    if (entry.accepted == 0U) {
      cue_ids.push_back(entry.cue_id);
    }
  }
  return cue_ids;
}

namespace {

auto worst_drop_reason(const CueTraceRecord& record) -> const char* {
  std::size_t worst = 0;
  std::uint64_t highest = 0;
  for (std::size_t index = 0; index < k_cue_outcome_count; ++index) {
    if (static_cast<CueOutcome>(index) == CueOutcome::Accepted) {
      continue;
    }
    if (record.outcomes.at(index) > highest) {
      highest = record.outcomes.at(index);
      worst = index;
    }
  }
  return highest == 0U ? "-" : cue_outcome_name(static_cast<CueOutcome>(worst));
}

} // namespace

auto format_status_overlay() -> std::string {
  auto& audio = AudioSystem::get_instance();
  auto& trace = CueTrace::instance();

  const auto last = trace.last_request();
  const auto entries = trace.records();

  std::uint64_t requests = 0;
  std::uint64_t accepted = 0;
  for (const auto& entry : entries) {
    requests += entry.requests;
    accepted += entry.accepted;
  }

  std::ostringstream out;
  out << std::fixed << std::setprecision(2);
  out << "AUDIO  master " << audio.get_master_volume() << "  sfx "
      << audio.get_sound_volume() << "  voice " << audio.get_voice_volume()
      << "  channels " << audio.get_active_channel_count() << "\n";

  if (last.cue_id.empty()) {
    out << "last   nothing requested yet\n";
  } else {
    out << "last   " << last.cue_id << " -> "
        << (last.resource_id.empty() ? "-" : last.resource_id) << "  "
        << cue_outcome_name(last.outcome) << "\n";
    out << "       from " << (last.source.empty() ? "-" : last.source) << " at "
        << std::setprecision(1) << last.seconds << "s\n"
        << std::setprecision(2);
  }

  out << "cues   " << entries.size() << " requested  " << accepted << " heard  "
      << (requests - accepted) << " dropped\n";

  const auto struggling = trace.struggling_cues();
  if (struggling.empty()) {
    out << "       every cue asked for is being heard";
  } else {
    out << "mostly silent:";
    std::size_t shown = 0;
    for (const auto& entry : struggling) {
      if (shown++ >= 5U) {
        out << "\n       ... and " << (struggling.size() - shown + 1U) << " more";
        break;
      }
      out << "\n       " << entry.cue_id << "  " << entry.accepted << "/"
          << entry.requests << "  " << worst_drop_reason(entry);
    }
  }
  return out.str();
}

auto CueTrace::write_summary(const std::string& path,
                             const std::string& label) const -> bool {
  const auto entries = records();

  std::uint64_t total_requests = 0;
  std::uint64_t total_accepted = 0;

  QJsonArray cues;
  QJsonArray never_accepted;
  for (const auto& entry : entries) {
    total_requests += entry.requests;
    total_accepted += entry.accepted;

    QJsonObject drops;
    for (std::size_t index = 0; index < k_cue_outcome_count; ++index) {
      const auto outcome = static_cast<CueOutcome>(index);
      if (outcome == CueOutcome::Accepted || entry.outcomes.at(index) == 0U) {
        continue;
      }
      drops.insert(QString::fromLatin1(cue_outcome_name(outcome)),
                   static_cast<qint64>(entry.outcomes.at(index)));
    }

    QJsonObject cue;
    cue.insert(QStringLiteral("cue"), QString::fromStdString(entry.cue_id));
    cue.insert(QStringLiteral("requests"), static_cast<qint64>(entry.requests));
    cue.insert(QStringLiteral("accepted"), static_cast<qint64>(entry.accepted));
    cue.insert(QStringLiteral("drops"), drops);
    QJsonArray sources;
    for (const auto& source : entry.sources) {
      sources.append(QString::fromStdString(source));
    }
    cue.insert(QStringLiteral("sources"), sources);
    QJsonObject resource_plays;
    for (const auto& [resource, plays] : entry.resource_plays) {
      resource_plays.insert(QString::fromStdString(resource),
                            static_cast<qint64>(plays));
    }
    cue.insert(QStringLiteral("resource_plays"), resource_plays);
    cue.insert(QStringLiteral("last_resource"),
               QString::fromStdString(entry.last_resource_id));
    cue.insert(QStringLiteral("last_request_seconds"), entry.last_request_seconds);
    cues.append(cue);

    if (entry.accepted == 0U) {
      never_accepted.append(QString::fromStdString(entry.cue_id));
    }
  }

  QJsonObject totals;
  totals.insert(QStringLiteral("cues_requested"), static_cast<qint64>(entries.size()));
  totals.insert(QStringLiteral("requests"), static_cast<qint64>(total_requests));
  totals.insert(QStringLiteral("accepted"), static_cast<qint64>(total_accepted));

  QJsonObject root;
  {
    std::lock_guard<std::mutex> const lock(m_mutex);
    root.insert(QStringLiteral("label"), QString::fromStdString(label));
    root.insert(QStringLiteral("duration_seconds"), elapsed_seconds_locked());
    root.insert(QStringLiteral("listener_mode"),
                QString::fromStdString(m_listener.mode));
  }
  root.insert(QStringLiteral("totals"), totals);
  root.insert(QStringLiteral("cues"), cues);
  root.insert(QStringLiteral("never_accepted"), never_accepted);

  const QString destination = QString::fromStdString(path);
  const QString directory = QFileInfo(destination).path();
  if (!directory.isEmpty()) {
    QDir().mkpath(directory);
  }

  QSaveFile file(destination);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "audio cue summary could not be written to" << destination;
    return false;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return file.commit();
}

auto CueTrace::write_requested_summary(const std::string& label) -> bool {
  const QString configured = summary_path_from_environment();
  if (configured.isEmpty()) {
    return false;
  }

  unsigned index = 0;
  {
    std::lock_guard<std::mutex> const lock(m_mutex);
    if (m_records.empty()) {
      return false;
    }
    index = m_exports++;
  }

  if (!write_summary(indexed_path(configured, index).toStdString(), label)) {
    return false;
  }

  reset();
  return true;
}

} // namespace Game::Audio
