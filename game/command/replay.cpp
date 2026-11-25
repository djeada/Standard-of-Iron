#include "replay.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <tuple>

#include "command_queue.h"
#include "json_u64.h"
#include "log_format.h"
#include "utils/resource_utils.h"

namespace Game::Command {

auto simulation_build_id() -> QString {
#ifdef SOI_SIMULATION_BUILD_ID
  return QStringLiteral(SOI_SIMULATION_BUILD_ID);
#else
  return QStringLiteral("unknown");
#endif
}

auto simulation_content_digest() -> QString {
  static const QString digest = []() -> QString {
    const QString data_root =
        Utils::Resources::resolve_resource_path(QStringLiteral("assets/data"));
    QDir data(data_root);
    if (!data.exists()) {
      return QStringLiteral("no-content");
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    QDirIterator files(data.absolutePath(),
                       QStringList{QStringLiteral("*.json")},
                       QDir::Files,
                       QDirIterator::Subdirectories);
    QStringList paths;
    while (files.hasNext()) {
      paths.append(files.next());
    }
    paths.sort();
    for (const QString& path : paths) {
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly)) {
        continue;
      }
      hash.addData(QDir(data.absolutePath()).relativeFilePath(path).toUtf8());
      hash.addData(file.readAll());
    }
    return QString::fromLatin1(hash.result().toHex().left(16));
  }();
  return digest;
}

auto ReplayHeader::compatibility_error() const -> QString {
  if (format_version != k_replay_format_version) {
    return QStringLiteral("replay format %1, this build reads %2")
        .arg(format_version)
        .arg(k_replay_format_version);
  }
  const QString current_build = simulation_build_id();
  if (build_id != current_build) {
    return QStringLiteral("recorded by simulation build %1, this is %2")
        .arg(build_id, current_build);
  }
  const QString current_content = simulation_content_digest();
  if (content_digest != current_content) {
    return QStringLiteral("recorded against content %1, this is %2")
        .arg(content_digest, current_content);
  }
  return {};
}

auto ReplayHeader::to_json() const -> QJsonObject {
  QJsonObject object;
  object["replay_format"] = format_version;
  object["build_id"] = build_id;
  object["content_digest"] = content_digest;
  object["kind"] = kind;
  object["reference"] = reference;
  object["launch"] = launch;
  object["tick_seconds"] = tick_seconds;
  object["rng_seed"] = u64_to_json(rng_seed);
  object["digest_interval"] = static_cast<int>(digest_interval);
  object["checkpoint_interval"] = static_cast<qint64>(checkpoint_interval);
  return object;
}

auto ReplayHeader::from_json(const QJsonObject& object) -> std::optional<ReplayHeader> {
  const auto version = object.value(QLatin1String("replay_format"));
  if (!version.isDouble() || version.toInt() < 1) {
    return std::nullopt;
  }
  ReplayHeader header;
  header.format_version = version.toInt();
  header.build_id = object.value(QLatin1String("build_id")).toString();
  header.content_digest = object.value(QLatin1String("content_digest")).toString();
  header.kind = object.value(QLatin1String("kind")).toString();
  header.reference = object.value(QLatin1String("reference")).toString();
  header.launch = object.value(QLatin1String("launch")).toObject();
  header.tick_seconds =
      object.value(QLatin1String("tick_seconds")).toDouble(1.0 / 60.0);
  const auto parsed_seed = u64_from_json(object.value(QLatin1String("rng_seed")));
  if (!parsed_seed || !std::isfinite(header.tick_seconds) ||
      header.tick_seconds <= 0.0) {
    return std::nullopt;
  }
  header.rng_seed = *parsed_seed;
  const auto interval_val = object.value(QLatin1String("digest_interval"));
  if (!interval_val.isUndefined()) {
    const qint64 interval = interval_val.toInteger(-1);
    if (!interval_val.isDouble() || interval < 0 ||
        interval > std::numeric_limits<std::uint32_t>::max()) {
      return std::nullopt;
    }
    header.digest_interval = static_cast<std::uint32_t>(interval);
  } else {
    header.digest_interval = 0;
  }
  const auto checkpoint_interval = object.value(QLatin1String("checkpoint_interval"));
  if (!checkpoint_interval.isUndefined()) {
    const auto interval = checkpoint_interval.toInteger(-1);
    if (interval < 0 || interval > std::numeric_limits<std::uint32_t>::max()) {
      return std::nullopt;
    }
    header.checkpoint_interval = static_cast<std::uint32_t>(interval);
  }
  return header;
}

ReplayRecorder::ReplayRecorder() = default;

ReplayRecorder::~ReplayRecorder() {
  finish();
}

auto ReplayRecorder::begin(const QString& path,
                           const ReplayHeader& header,
                           CommandQueue& queue,
                           CaptureCheckpoint capture_checkpoint) -> bool {
  finish();
  auto file = std::make_unique<QFile>(path);
  auto metadata = header;
  if (!capture_checkpoint) {
    metadata.checkpoint_interval = 0;
  }
  constexpr qint64 magic_bytes = sizeof(k_log_magic) - 1;
  if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file->write(k_log_magic, magic_bytes) != magic_bytes ||
      !write_log_frame(*file, metadata.to_json()) || !file->flush()) {
    return false;
  }
  m_file = std::move(file);
  m_path = path;
  m_count = 0;
  m_failed = false;
  m_digest_interval = header.digest_interval;
  m_checkpoint_interval = metadata.checkpoint_interval;
  m_capture_checkpoint = std::move(capture_checkpoint);
  m_log = &queue.log();
  m_log->attach_writer([this](const LogEntry& entry) { write(entry); },
                       [this]() { flush(); });
  if (m_failed) {
    finish();
    return false;
  }
  return true;
}

void ReplayRecorder::write(const LogEntry& entry) {
  if (m_failed) {
    return;
  }
  if (!write_log_frame(*m_file, to_json(entry))) {
    m_failed = true;
    qWarning() << "Replay: cannot write log record to" << m_path;
  } else if (std::holds_alternative<Payload>(entry.payload)) {
    ++m_count;
  }
}

void ReplayRecorder::flush() {
  if (!m_failed && !m_file->flush()) {
    m_failed = true;
    qWarning() << "Replay: cannot flush" << m_path << m_file->errorString();
  }
}

void ReplayRecorder::finish() {
  if (m_log != nullptr) {
    m_log->finish_match();
    m_log->detach_writer();
    m_log = nullptr;
  }
  m_file.reset();
  m_capture_checkpoint = {};
}

auto ReplayRecorder::recording() const -> bool {
  return m_file != nullptr && !m_failed;
}

auto ReplayRecorder::digest_due(std::uint64_t tick) const -> bool {
  return recording() && m_digest_interval != 0 && tick % m_digest_interval == 0;
}

auto ReplayRecorder::checkpoint_at(std::uint64_t tick) const
    -> std::optional<Checkpoint> {
  if (!recording() || m_checkpoint_interval == 0 || tick % m_checkpoint_interval != 0) {
    return std::nullopt;
  }
  auto snapshot = m_capture_checkpoint();
  if (snapshot.isEmpty()) {
    return std::nullopt;
  }
  return Checkpoint{std::move(snapshot)};
}

namespace {

auto older_format_error(QFile& file) -> QString {
  file.seek(0);
  const auto header = QJsonDocument::fromJson(file.readLine(64 * 1024)).object();
  const int version = header.value(QLatin1String("replay_format")).toInt();
  if (version > 0) {
    const auto build = header.value(QLatin1String("build_id")).toString();
    return QStringLiteral(
               "replay format %1 requires its original simulation build (%2)")
        .arg(version)
        .arg(build.isEmpty() ? QStringLiteral("build ID unavailable") : build);
  }
  return QStringLiteral("missing command log signature");
}

template <typename T>
auto count_records(const CommandLog::Entries& records) -> std::size_t {
  return static_cast<std::size_t>(std::ranges::count_if(records, [](const auto& entry) {
    return std::holds_alternative<T>(entry->payload);
  }));
}

auto digest_at(const ReplayFile& file,
               std::uint64_t tick) -> const Game::Session::SubsystemDigests* {
  for (const auto& entry : file.entries_at(tick)) {
    if (const auto* digest =
            std::get_if<Game::Session::SubsystemDigests>(&entry->payload)) {
      return digest;
    }
  }
  return nullptr;
}

} // namespace

auto ReplayFile::load(const QString& path,
                      QString* error) -> std::optional<ReplayFile> {
  const auto fail = [&](const QString& message) -> std::optional<ReplayFile> {
    if (error != nullptr) {
      *error = QStringLiteral("%1: %2").arg(path, message);
    }
    return std::nullopt;
  };
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return fail(file.errorString());
  }
  if (file.read(sizeof(k_log_magic) - 1) != k_log_magic) {
    return fail(older_format_error(file));
  }
  QJsonObject object;
  if (read_log_frame(file, object) != FrameStatus::Valid) {
    return fail(QStringLiteral("invalid command log header"));
  }
  auto header = ReplayHeader::from_json(object);
  if (!header) {
    return fail(QStringLiteral("invalid replay metadata"));
  }
  if (const auto incompatible = header->compatibility_error();
      !incompatible.isEmpty()) {
    return fail(incompatible);
  }

  ReplayFile replay;
  replay.header = std::move(*header);
  std::uint64_t last_seq = 0;
  bool ended = false;
  for (;;) {
    const auto offset = file.pos();
    const auto status = read_log_frame(file, object);
    if (status == FrameStatus::End || status == FrameStatus::Truncated) {
      replay.recovered_tail = !ended || status == FrameStatus::Truncated;
      break;
    }
    auto entry =
        status == FrameStatus::Valid ? log_entry_from_json(object) : std::nullopt;
    if (!entry || ended || entry->seq <= last_seq) {
      return fail(QStringLiteral("invalid command log record at byte %1").arg(offset));
    }
    last_seq = entry->seq;
    if (std::holds_alternative<TickComplete>(entry->payload)) {
      if (entry->tick != replay.completed_tick + 1) {
        return fail(
            QStringLiteral("missing tick before record at byte %1").arg(offset));
      }
      replay.completed_tick = entry->tick;
    } else if (std::holds_alternative<MatchEnd>(entry->payload)) {
      if (entry->tick != replay.completed_tick) {
        return fail(QStringLiteral("invalid match end at byte %1").arg(offset));
      }
      ended = true;
    } else if (entry->tick <= replay.completed_tick) {
      return fail(
          QStringLiteral("input arrived after its tick closed at byte %1").arg(offset));
    }
    replay.records.push_back(std::make_shared<const LogEntry>(std::move(*entry)));
  }

  // A crash can leave complete frames for an unfinished tick, as can input delay
  // when a match ends. Only a completed tick is safe to replay.
  std::erase_if(replay.records, [&replay](const auto& entry) {
    return entry->tick > replay.completed_tick;
  });
  std::ranges::sort(replay.records, {}, [](const auto& entry) {
    return std::tie(entry->tick, entry->seq);
  });
  return replay;
}

auto ReplayFile::entries_at(std::uint64_t tick) const
    -> std::span<const CommandLog::Entries::value_type> {
  const auto entries = std::ranges::equal_range(
      records, tick, {}, [](const auto& entry) { return entry->tick; });
  return {entries.begin(), entries.end()};
}

auto ReplayFile::command_count() const -> std::size_t {
  return count_records<Payload>(records);
}

auto ReplayFile::digest_count() const -> std::size_t {
  return count_records<Game::Session::SubsystemDigests>(records);
}

auto ReplayFile::checkpoint_at_or_before(std::uint64_t tick) const -> const LogEntry* {
  auto end = std::ranges::upper_bound(
      records, tick, {}, [](const auto& entry) { return entry->tick; });
  while (end != records.begin()) {
    const auto& entry = *--end;
    if (std::holds_alternative<Checkpoint>(entry->payload)) {
      return entry.get();
    }
  }
  return nullptr;
}

ReplayPlayer::ReplayPlayer(ReplayFile file)
    : m_file(std::move(file)) {
}

auto ReplayPlayer::digest_due(std::uint64_t tick) const -> bool {
  return digest_at(m_file, tick) != nullptr;
}

auto ReplayPlayer::check(std::uint64_t tick,
                         const Game::Session::SubsystemDigests& digests) -> bool {
  const auto* recorded = digest_at(m_file, tick);
  if (recorded == nullptr) {
    return true;
  }
  ++m_checked;
  const char* subsystem = Game::Session::name_of_first_difference(*recorded, digests);
  if (subsystem == nullptr) {
    return true;
  }
  if (!m_divergence) {
    m_divergence = ReplayDivergence{.tick = tick,
                                    .recorded = recorded->root,
                                    .observed = digests.root,
                                    .subsystem = subsystem};
  }
  return false;
}

void ReplayPlayer::feed(std::uint64_t tick, CommandQueue& queue) {
  while (m_next < m_file.records.size() && m_file.records[m_next]->tick <= tick) {
    const auto& entry = *m_file.records[m_next++];
    if (entry.tick == tick && queue.log().append_replicated(entry) &&
        std::holds_alternative<Payload>(entry.payload)) {
      ++m_fed;
    }
  }
}

} // namespace Game::Command
