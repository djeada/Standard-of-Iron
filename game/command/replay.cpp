#include "replay.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

#include "command_codec.h"
#include "command_queue.h"
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
  if (!build_id.isEmpty() && build_id != current_build) {
    return QStringLiteral("recorded by simulation build %1, this is %2")
        .arg(build_id, current_build);
  }
  const QString current_content = simulation_content_digest();
  if (!content_digest.isEmpty() && content_digest != current_content) {
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
  object["rng_seed"] = static_cast<qint64>(rng_seed);
  object["digest_interval"] = static_cast<int>(digest_interval);
  return object;
}

auto ReplayHeader::from_json(const QJsonObject& object) -> std::optional<ReplayHeader> {
  const auto version = object.value(QLatin1String("replay_format"));
  if (!version.isDouble() || version.toInt() != k_replay_format_version) {
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
  header.rng_seed =
      static_cast<std::uint64_t>(object.value(QLatin1String("rng_seed")).toDouble(0.0));
  header.digest_interval = static_cast<std::uint32_t>(
      object.value(QLatin1String("digest_interval")).toInt(0));
  return header;
}

ReplayRecorder::ReplayRecorder() = default;

ReplayRecorder::~ReplayRecorder() {
  finish();
}

auto ReplayRecorder::begin(const QString& path,
                           const ReplayHeader& header,
                           CommandQueue& queue) -> bool {
  finish();
  auto file = std::make_unique<QFile>(path);
  if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    return false;
  }
  file->write(QJsonDocument(header.to_json()).toJson(QJsonDocument::Compact));
  file->write("\n");
  m_file = std::move(file);
  m_path = path;
  m_count = 0;
  m_digest_interval = header.digest_interval;
  m_queue = &queue;
  queue.set_observer([this](const Command& command) { record(command); });
  return true;
}

void ReplayRecorder::record(const Command& command) {
  if (!m_file) {
    return;
  }
  m_file->write(QJsonDocument(to_json(command)).toJson(QJsonDocument::Compact));
  m_file->write("\n");
  ++m_count;
}

void ReplayRecorder::record_digest(std::uint64_t tick, std::uint64_t digest) {
  if (!m_file || m_digest_interval == 0 || tick % m_digest_interval != 0) {
    return;
  }
  QJsonObject object;
  object["digest"] = QString::number(digest);
  object["tick"] = static_cast<qint64>(tick);
  m_file->write(QJsonDocument(object).toJson(QJsonDocument::Compact));
  m_file->write("\n");
}

void ReplayRecorder::finish() {
  if (m_queue != nullptr) {
    m_queue->set_observer({});
    m_queue = nullptr;
  }
  if (m_file) {
    m_file->flush();
    m_file->close();
    m_file.reset();
  }
}

auto ReplayRecorder::recording() const -> bool {
  return m_file != nullptr;
}

auto ReplayFile::load(const QString& path,
                      QString* error) -> std::optional<ReplayFile> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error != nullptr) {
      *error = QStringLiteral("cannot open %1").arg(path);
    }
    return std::nullopt;
  }
  QTextStream in(&file);
  ReplayFile replay;
  int line_number = 0;
  bool header_read = false;
  while (!in.atEnd()) {
    const QString line = in.readLine().trimmed();
    ++line_number;
    if (line.isEmpty()) {
      continue;
    }
    QJsonParseError parse_error{};
    const auto document = QJsonDocument::fromJson(line.toUtf8(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
      if (error != nullptr) {
        *error = QStringLiteral("%1:%2: not a JSON object").arg(path).arg(line_number);
      }
      return std::nullopt;
    }
    if (!header_read) {
      auto header = ReplayHeader::from_json(document.object());
      if (!header.has_value()) {
        if (error != nullptr) {
          *error = QStringLiteral("%1:%2: not a replay header this build reads")
                       .arg(path)
                       .arg(line_number);
        }
        return std::nullopt;
      }
      if (const QString incompatible = header->compatibility_error();
          !incompatible.isEmpty()) {
        if (error != nullptr) {
          *error = QStringLiteral("%1: cannot be replayed by this build (%2)")
                       .arg(path, incompatible);
        }
        return std::nullopt;
      }
      replay.header = std::move(*header);
      header_read = true;
      continue;
    }
    const auto object = document.object();
    if (object.contains(QLatin1String("digest"))) {
      bool ok = false;
      const auto digest =
          object.value(QLatin1String("digest")).toString().toULongLong(&ok);
      const auto tick = object.value(QLatin1String("tick"));
      if (!ok || !tick.isDouble()) {
        if (error != nullptr) {
          *error =
              QStringLiteral("%1:%2: malformed digest line").arg(path).arg(line_number);
        }
        return std::nullopt;
      }
      replay.digests.push_back({static_cast<std::uint64_t>(tick.toDouble()), digest});
      continue;
    }
    auto command = from_json(object);
    if (!command.has_value()) {
      if (error != nullptr) {
        *error = QStringLiteral("%1:%2: not a command this build applies")
                     .arg(path)
                     .arg(line_number);
      }
      return std::nullopt;
    }
    replay.commands.push_back(std::move(*command));
  }
  if (!header_read) {
    if (error != nullptr) {
      *error = QStringLiteral("%1: empty replay").arg(path);
    }
    return std::nullopt;
  }
  return replay;
}

auto ReplayFile::last_tick() const -> std::uint64_t {
  return commands.empty() ? 0 : commands.back().submitted_tick;
}

ReplayPlayer::ReplayPlayer(ReplayFile file)
    : m_file(std::move(file)) {

  std::stable_sort(m_file.commands.begin(),
                   m_file.commands.end(),
                   [](const Command& a, const Command& b) {
                     return a.submitted_tick < b.submitted_tick;
                   });
}

auto ReplayPlayer::check(std::uint64_t tick, std::uint64_t digest) -> bool {
  while (m_next_digest < m_file.digests.size() &&
         m_file.digests[m_next_digest].tick < tick) {
    ++m_next_digest;
  }
  if (m_next_digest >= m_file.digests.size() ||
      m_file.digests[m_next_digest].tick != tick) {
    return true;
  }
  const auto recorded = m_file.digests[m_next_digest].digest;
  ++m_next_digest;
  ++m_checked;
  if (recorded == digest) {
    return true;
  }
  if (!m_divergence.has_value()) {
    m_divergence =
        ReplayDivergence{.tick = tick, .recorded = recorded, .observed = digest};
  }
  return false;
}

void ReplayPlayer::feed(std::uint64_t tick, CommandQueue& queue) {
  while (m_next < m_file.commands.size() &&
         m_file.commands[m_next].submitted_tick <= tick) {
    Command command = m_file.commands[m_next];
    command.source = Source::Replay;
    queue.submit(std::move(command));
    ++m_next;
  }
}

} // namespace Game::Command
