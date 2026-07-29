#include "command_queue.h"

#include "../core/world.h"
#include "../session/session_context.h"
#include "command_dispatcher.h"

namespace Game::Command {

void CommandQueue::submit(Command command) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_pending.push_back(std::move(command));
}

void CommandQueue::submit(Source source, int owner_id, Payload payload) {
  submit(
      Command{.source = source, .owner_id = owner_id, .payload = std::move(payload)});
}

auto CommandQueue::drain(Engine::Core::World& world,
                         std::uint64_t tick) -> std::size_t {

  std::deque<Command> batch;
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    batch.swap(m_pending);
  }

  std::size_t executed = 0;
  for (auto& command : batch) {
    command.submitted_tick = tick;

    auto validation = validate(world, command);
    if (!validation.accepted()) {
      ++m_rejected;
      if (m_rejection_observer) {
        m_rejection_observer(command, validation.rejection);
      }
      continue;
    }

    ++m_accepted;
    if (m_observer) {
      m_observer(validation.command);
    }
    dispatch(world, validation.command);
    ++executed;
  }

  return executed;
}

auto CommandQueue::pending() const -> std::size_t {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_pending.size();
}

void CommandQueue::clear() {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_pending.clear();
  m_accepted = 0;
  m_rejected = 0;
}

void submit(Engine::Core::World& world, Source source, int owner_id, Payload payload) {
  if (auto* session = Game::Session::SessionContext::for_world(world)) {
    session->commands().submit(source, owner_id, std::move(payload));
    return;
  }

  const Command command{
      .source = source, .owner_id = owner_id, .payload = std::move(payload)};
  const auto validation = validate(world, command);
  if (validation.accepted()) {
    dispatch(world, validation.command);
  }
}

} // namespace Game::Command
