#pragma once

#include <QByteArray>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>

#include "../session/world_digest.h"
#include "command.h"
#include "commander_input.h"

namespace Game::Command {

struct ClockChange {
  std::optional<bool> paused;
  std::optional<double> speed;
};

struct Checkpoint {
  QByteArray snapshot;
};

struct TickComplete {};
struct MatchEnd {};

using LogPayload = std::variant<Payload,
                                CommanderInputFrame,
                                CommanderActionRecord,
                                ClockChange,
                                Game::Session::SubsystemDigests,
                                Checkpoint,
                                TickComplete,
                                MatchEnd>;

struct LogEntry {
  std::uint64_t tick;
  std::uint64_t seq;
  int owner;
  Source source;
  LogPayload payload;
};

class CommandLog {
public:
  using Entries = std::vector<std::shared_ptr<const LogEntry>>;
  using Writer = std::function<void(const LogEntry&)>;
  using Flusher = std::function<void()>;

  auto submit(Source source, int owner, LogPayload payload) -> bool;
  void append(std::uint64_t tick, Source source, int owner, LogPayload payload);
  auto append_replicated(const LogEntry& entry) -> bool;

  [[nodiscard]] auto entries_at(std::uint64_t tick) const -> Entries;
  auto seal_inputs(std::uint64_t tick) -> Entries;
  // Removes and returns the orders submitted but not yet sealed into a tick,
  // without consuming a tick number. A stopped clock runs orders now; taking
  // them through seal_inputs would burn one tick number per paused frame and
  // push every later order that far into the future.
  auto take_unsealed_inputs() -> Entries;
  void complete_tick(std::uint64_t tick);
  void finish_match();

  void set_input_delay(std::uint32_t ticks);
  void set_follower(bool follower);
  [[nodiscard]] auto follower() const -> bool;
  [[nodiscard]] auto available_tick() const -> std::uint64_t;
  [[nodiscard]] auto pending_commands() const -> std::size_t;
  [[nodiscard]] auto dropped_inputs() const -> std::uint64_t;
  void reset(std::uint64_t completed_tick = 0);

  void attach_writer(Writer write, Flusher flush);
  void detach_writer();
  void flush();

private:
  void append_locked(LogEntry entry);
  void flush_locked();

  mutable std::mutex m_mutex;
  std::map<std::uint64_t, Entries> m_ticks;
  std::uint64_t m_next_input_tick = 1;
  std::uint64_t m_next_seq = 1;
  std::uint64_t m_completed_tick = 0;
  std::uint64_t m_replicated_through = 0;
  std::uint64_t m_dropped_inputs = 0;
  std::uint32_t m_input_delay = 0;
  bool m_follower = false;
  bool m_ended = false;
  Writer m_write;
  Flusher m_flush;
};

} // namespace Game::Command
