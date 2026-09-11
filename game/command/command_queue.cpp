#include "command_queue.h"

#include <QDebug>

#include <atomic>

#include "../core/world.h"
#include "../session/session_context.h"
#include "command_dispatcher.h"

namespace Game::Command {

void CommandQueue::submit(Command command) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (m_replay_only && command.source != Source::Replay) {
    ++m_dropped;
    return;
  }
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

namespace {
std::atomic<std::uint64_t> g_unqueued_submissions{0};
thread_local bool t_immediate_dispatch = false;
} // namespace

ScopedImmediateDispatch::ScopedImmediateDispatch()
    : m_previous(t_immediate_dispatch) {
  t_immediate_dispatch = true;
}

ScopedImmediateDispatch::~ScopedImmediateDispatch() {
  t_immediate_dispatch = m_previous;
}

auto immediate_dispatch_allowed() -> bool {
  return t_immediate_dispatch;
}

auto submit(Engine::Core::World& world,
            Source source,
            int owner_id,
            Payload payload) -> bool {
  auto* session = Game::Session::SessionContext::for_world(world);
  if (session == nullptr) {
    if (t_immediate_dispatch) {
      return dispatch_immediately(world, source, owner_id, std::move(payload));
    }
    g_unqueued_submissions.fetch_add(1, std::memory_order_relaxed);
    qWarning() << "Game::Command::submit: the world has no session, so there is no "
                  "queue to put this order in; it was refused. Bind the world to a "
                  "SessionContext, or wrap the call in a "
                  "Game::Command::ScopedImmediateDispatch if you mean to skip the "
                  "queue.";
    return false;
  }
  session->commands().submit(source, owner_id, std::move(payload));
  return true;
}

auto dispatch_immediately(Engine::Core::World& world,
                          Source source,
                          int owner_id,
                          Payload payload) -> bool {
  const Command command{
      .source = source, .owner_id = owner_id, .payload = std::move(payload)};
  const auto validation = validate(world, command);
  if (!validation.accepted()) {
    return false;
  }
  dispatch(world, validation.command);
  return true;
}

auto unqueued_submissions() -> std::uint64_t {
  return g_unqueued_submissions.load(std::memory_order_relaxed);
}

void reset_unqueued_submissions() {
  g_unqueued_submissions.store(0, std::memory_order_relaxed);
}

} // namespace Game::Command
