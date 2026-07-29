#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <vector>

#include "command.h"
#include "command_validator.h"

namespace Engine::Core {
class World;
}

namespace Game::Command {

class CommandQueue {
public:
  using Observer = std::function<void(const Command&)>;

  using RejectionObserver = std::function<void(const Command&, Rejection)>;

  CommandQueue() = default;

  CommandQueue(const CommandQueue&) = delete;
  CommandQueue(CommandQueue&&) = delete;
  auto operator=(const CommandQueue&) -> CommandQueue& = delete;
  auto operator=(CommandQueue&&) -> CommandQueue& = delete;

  void submit(Command command);

  void submit(Source source, int owner_id, Payload payload);

  auto drain(Engine::Core::World& world, std::uint64_t tick) -> std::size_t;

  [[nodiscard]] auto pending() const -> std::size_t;

  void clear();

  void set_observer(Observer observer) { m_observer = std::move(observer); }
  void set_rejection_observer(RejectionObserver observer) {
    m_rejection_observer = std::move(observer);
  }

  [[nodiscard]] auto accepted_count() const -> std::uint64_t { return m_accepted; }
  [[nodiscard]] auto rejected_count() const -> std::uint64_t { return m_rejected; }

private:
  mutable std::mutex m_mutex;
  std::deque<Command> m_pending;
  Observer m_observer;
  RejectionObserver m_rejection_observer;
  std::uint64_t m_accepted = 0;
  std::uint64_t m_rejected = 0;
};

void submit(Engine::Core::World& world, Source source, int owner_id, Payload payload);

} // namespace Game::Command
