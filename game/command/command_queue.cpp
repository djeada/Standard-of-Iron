#include "command_queue.h"

#include <QDebug>

#include <atomic>

#include "../core/world.h"
#include "../session/session_context.h"
#include "command_dispatcher.h"

namespace Game::Command {

auto CommandQueue::submit(Command command) -> bool {
  return submit(command.source, command.owner_id, std::move(command.payload));
}

auto CommandQueue::submit(Source source, int owner_id, Payload payload) -> bool {
  return m_log.submit(source, owner_id, std::move(payload));
}

auto CommandQueue::drain(Engine::Core::World& world,
                         std::uint64_t tick) -> std::size_t {

  std::size_t executed = 0;
  for (const auto& entry : m_log.seal_inputs(tick)) {
    const auto* payload = std::get_if<Payload>(&entry->payload);
    if (payload == nullptr) {
      continue;
    }
    const Command command{.source = entry->source,
                          .owner_id = entry->owner,
                          .submitted_tick = entry->tick,
                          .payload = *payload,
                          .replayed = m_log.follower()};
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
  return m_log.pending_commands();
}

void CommandQueue::clear(std::uint64_t completed_tick) {
  m_log.reset(completed_tick);
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
  return session->commands().submit(source, owner_id, std::move(payload));
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
