#include "save_orchestrator.h"

#include <chrono>
#include <utility>

namespace App::Core {

auto SaveOrchestrator::queue(const QString& slot,
                             Game::Systems::Save::SlotKind kind,
                             int autosave_retention) -> bool {
  if (!m_callbacks.simulation_running || !m_callbacks.simulation_running()) {
    return false;
  }
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (m_request.valid) {
    return false;
  }
  m_request = Request{.slot = slot,
                      .kind = kind,
                      .autosave_retention = autosave_retention,
                      .valid = true};
  return true;
}

void SaveOrchestrator::drain() {
  Request request;
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_request.valid) {
      return;
    }
    request = m_request;
    m_request.valid = false;
  }

  if (!m_callbacks.capture) {
    return;
  }

  const auto started = std::chrono::steady_clock::now();
  const SaveToSlotEffects effects =
      m_callbacks.capture(request.slot, request.kind, request.autosave_retention);
  m_last_capture_us.store(
      static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - started)
                                     .count()),
      std::memory_order_release);

  if (m_callbacks.deliver) {
    m_callbacks.deliver(request.slot, effects);
  }
}

auto SaveOrchestrator::cancel_queued() -> bool {
  const std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_request.valid) {
    return false;
  }
  m_request.valid = false;
  return true;
}

auto SaveOrchestrator::queued() const -> bool {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_request.valid;
}

} // namespace App::Core
