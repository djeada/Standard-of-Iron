#pragma once

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "command_log.h"

class QFile;

namespace Game::Command {

class CommandQueue;

inline constexpr int k_replay_format_version = 8;

[[nodiscard]] auto simulation_build_id() -> QString;

[[nodiscard]] auto simulation_content_digest() -> QString;

struct ReplayHeader {
  int format_version = k_replay_format_version;
  QString kind;
  QString reference;
  QJsonObject launch;
  double tick_seconds = 1.0 / 60.0;
  std::uint64_t rng_seed = 0;
  std::uint32_t digest_interval = 30;
  std::uint32_t checkpoint_interval = 3600;
  QString build_id = simulation_build_id();
  QString content_digest = simulation_content_digest();

  [[nodiscard]] auto to_json() const -> QJsonObject;
  [[nodiscard]] static auto
  from_json(const QJsonObject& object) -> std::optional<ReplayHeader>;

  [[nodiscard]] auto compatibility_error() const -> QString;
};

class ReplayRecorder {
public:
  using CaptureCheckpoint = std::function<QByteArray()>;

  ReplayRecorder();
  ~ReplayRecorder();
  ReplayRecorder(const ReplayRecorder&) = delete;
  auto operator=(const ReplayRecorder&) -> ReplayRecorder& = delete;

  auto begin(const QString& path,
             const ReplayHeader& header,
             CommandQueue& queue,
             CaptureCheckpoint capture_checkpoint = {}) -> bool;

  void finish();

  [[nodiscard]] auto recording() const -> bool;
  [[nodiscard]] auto failed() const -> bool { return m_failed; }
  [[nodiscard]] auto path() const -> const QString& { return m_path; }
  [[nodiscard]] auto recorded_count() const -> std::uint64_t { return m_count; }
  [[nodiscard]] auto digest_due(std::uint64_t tick) const -> bool;
  [[nodiscard]] auto
  checkpoint_at(std::uint64_t tick) const -> std::optional<Checkpoint>;

private:
  void write(const LogEntry& entry);
  void flush();

  QString m_path;
  std::unique_ptr<QFile> m_file;
  CommandLog* m_log = nullptr;
  std::uint64_t m_count = 0;
  std::uint32_t m_digest_interval = 0;
  std::uint32_t m_checkpoint_interval = 0;
  CaptureCheckpoint m_capture_checkpoint;
  bool m_failed = false;
};

struct ReplayDivergence {
  std::uint64_t tick = 0;
  std::uint64_t recorded = 0;
  std::uint64_t observed = 0;

  const char* subsystem = nullptr;
};

struct ReplayFile {
  ReplayHeader header;
  CommandLog::Entries records;
  std::uint64_t completed_tick = 0;
  bool recovered_tail = false;

  static auto load(const QString& path,
                   QString* error = nullptr) -> std::optional<ReplayFile>;

  [[nodiscard]] auto last_tick() const -> std::uint64_t { return completed_tick; }
  [[nodiscard]] auto
  checkpoint_at_or_before(std::uint64_t tick) const -> const LogEntry*;
  [[nodiscard]] auto entries_at(std::uint64_t tick) const
      -> std::span<const CommandLog::Entries::value_type>;
  [[nodiscard]] auto command_count() const -> std::size_t;
  [[nodiscard]] auto digest_count() const -> std::size_t;
};

class ReplayPlayer {
public:
  explicit ReplayPlayer(ReplayFile file);

  void feed(std::uint64_t tick, CommandQueue& queue);
  void resume_after(std::uint64_t tick);

  auto check(std::uint64_t tick,
             const Game::Session::SubsystemDigests& digests) -> bool;

  [[nodiscard]] auto digest_due(std::uint64_t tick) const -> bool;

  [[nodiscard]] auto finished() const -> bool {
    return m_next >= m_file.records.size();
  }
  [[nodiscard]] auto fed_count() const -> std::size_t { return m_fed; }
  [[nodiscard]] auto checked_count() const -> std::size_t { return m_checked; }
  [[nodiscard]] auto divergence() const -> const std::optional<ReplayDivergence>& {
    return m_divergence;
  }
  [[nodiscard]] auto file() const -> const ReplayFile& { return m_file; }

private:
  ReplayFile m_file;
  std::size_t m_next = 0;
  std::size_t m_fed = 0;
  std::size_t m_checked = 0;
  std::optional<ReplayDivergence> m_divergence;
};

} // namespace Game::Command
