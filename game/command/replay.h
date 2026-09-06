#pragma once

#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "command.h"
#include "commander_input.h"

class QFile;

namespace Game::Command {

class CommandQueue;

inline constexpr int k_replay_format_version = 3;

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
  QString build_id = simulation_build_id();
  QString content_digest = simulation_content_digest();

  [[nodiscard]] auto to_json() const -> QJsonObject;
  [[nodiscard]] static auto
  from_json(const QJsonObject& object) -> std::optional<ReplayHeader>;

  [[nodiscard]] auto compatibility_error() const -> QString;
};

class ReplayRecorder {
public:
  ReplayRecorder();
  ~ReplayRecorder();
  ReplayRecorder(const ReplayRecorder&) = delete;
  auto operator=(const ReplayRecorder&) -> ReplayRecorder& = delete;

  auto
  begin(const QString& path, const ReplayHeader& header, CommandQueue& queue) -> bool;

  void record(const Command& command);

  void record_digest(std::uint64_t tick, std::uint64_t digest);

  void record_commander_input(std::uint64_t tick, const CommanderInputFrame& frame);

  void finish();

  [[nodiscard]] auto recording() const -> bool;
  [[nodiscard]] auto path() const -> const QString& { return m_path; }
  [[nodiscard]] auto recorded_count() const -> std::uint64_t { return m_count; }

private:
  QString m_path;
  std::unique_ptr<QFile> m_file;
  CommandQueue* m_queue = nullptr;
  std::uint64_t m_count = 0;
  std::uint32_t m_digest_interval = 0;
};

struct RecordedDigest {
  std::uint64_t tick = 0;
  std::uint64_t digest = 0;
};

struct RecordedCommanderInput {
  std::uint64_t tick = 0;
  CommanderInputFrame frame;
};

struct ReplayDivergence {
  std::uint64_t tick = 0;
  std::uint64_t recorded = 0;
  std::uint64_t observed = 0;
};

struct ReplayFile {
  ReplayHeader header;
  std::vector<Command> commands;
  std::vector<RecordedDigest> digests;
  std::vector<RecordedCommanderInput> commander_inputs;

  static auto load(const QString& path,
                   QString* error = nullptr) -> std::optional<ReplayFile>;

  [[nodiscard]] auto last_tick() const -> std::uint64_t;
};

class ReplayPlayer {
public:
  explicit ReplayPlayer(ReplayFile file);

  void feed(std::uint64_t tick, CommandQueue& queue);

  auto check(std::uint64_t tick, std::uint64_t digest) -> bool;

  [[nodiscard]] auto
  commander_input(std::uint64_t tick) const -> const CommanderInputFrame*;

  [[nodiscard]] auto commander_input_count() const -> std::size_t {
    return m_file.commander_inputs.size();
  }

  [[nodiscard]] auto finished() const -> bool {
    return m_next >= m_file.commands.size();
  }
  [[nodiscard]] auto fed_count() const -> std::size_t { return m_next; }
  [[nodiscard]] auto checked_count() const -> std::size_t { return m_checked; }
  [[nodiscard]] auto divergence() const -> const std::optional<ReplayDivergence>& {
    return m_divergence;
  }
  [[nodiscard]] auto file() const -> const ReplayFile& { return m_file; }

private:
  ReplayFile m_file;
  std::size_t m_next = 0;
  std::size_t m_next_digest = 0;
  std::size_t m_checked = 0;
  std::optional<ReplayDivergence> m_divergence;
};

} // namespace Game::Command
