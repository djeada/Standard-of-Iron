#pragma once

#include <cstdint>
#include <functional>

#include "command_log.h"
#include "command_validator.h"

namespace Engine::Core {
class World;
}

namespace Game::Command {

class CommandQueue {
public:
  using Observer = std::function<void(const Command&)>;
  using RecordHandler = std::function<void(const LogEntry&)>;

  using RejectionObserver = std::function<void(const Command&, Rejection)>;

  CommandQueue() = default;

  CommandQueue(const CommandQueue&) = delete;
  CommandQueue(CommandQueue&&) = delete;
  auto operator=(const CommandQueue&) -> CommandQueue& = delete;
  auto operator=(CommandQueue&&) -> CommandQueue& = delete;

  auto submit(Command command) -> bool;

  auto submit(Source source, int owner_id, Payload payload) -> bool;

  auto drain(Engine::Core::World& world,
             std::uint64_t tick,
             const RecordHandler& handle_record = {}) -> std::size_t;

  // Runs every order submitted but not yet sealed, without advancing the log's
  // tick. For a paused match, whose orders take effect now.
  auto drain_stopped(Engine::Core::World& world) -> std::size_t;

  [[nodiscard]] auto pending() const -> std::size_t;

  void clear(std::uint64_t completed_tick = 0);

  [[nodiscard]] auto log() -> CommandLog& { return m_log; }
  [[nodiscard]] auto log() const -> const CommandLog& { return m_log; }

  void set_observer(Observer observer) { m_observer = std::move(observer); }
  void set_rejection_observer(RejectionObserver observer) {
    m_rejection_observer = std::move(observer);
  }

  void set_replay_only(bool replay_only) { m_log.set_follower(replay_only); }
  [[nodiscard]] auto replay_only() const -> bool { return m_log.follower(); }
  [[nodiscard]] auto dropped_count() const -> std::uint64_t {
    return m_log.dropped_inputs();
  }

  [[nodiscard]] auto accepted_count() const -> std::uint64_t { return m_accepted; }
  [[nodiscard]] auto rejected_count() const -> std::uint64_t { return m_rejected; }

private:
  auto execute(Engine::Core::World& world,
               const CommandLog::Entries& entries,
               const RecordHandler& handle_record) -> std::size_t;

  CommandLog m_log;
  Observer m_observer;
  RejectionObserver m_rejection_observer;
  std::uint64_t m_accepted = 0;
  std::uint64_t m_rejected = 0;
};

auto submit(Engine::Core::World& world,
            Source source,
            int owner_id,
            Payload payload) -> bool;

auto dispatch_immediately(Engine::Core::World& world,
                          Source source,
                          int owner_id,
                          Payload payload) -> bool;

class ScopedImmediateDispatch {
public:
  ScopedImmediateDispatch();
  ~ScopedImmediateDispatch();

  ScopedImmediateDispatch(const ScopedImmediateDispatch&) = delete;
  ScopedImmediateDispatch(ScopedImmediateDispatch&&) = delete;
  auto operator=(const ScopedImmediateDispatch&) -> ScopedImmediateDispatch& = delete;
  auto operator=(ScopedImmediateDispatch&&) -> ScopedImmediateDispatch& = delete;

private:
  bool m_previous;
};

[[nodiscard]] auto immediate_dispatch_allowed() -> bool;

[[nodiscard]] auto unqueued_submissions() -> std::uint64_t;

void reset_unqueued_submissions();

} // namespace Game::Command
