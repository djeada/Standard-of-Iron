#include "app/core/deferred_presentation_queue.h"

#include <utility>

namespace App::Core {

void DeferredPresentationQueue::post(Job job) {
  if (!job) {
    return;
  }
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_pending.push_back(std::move(job));
}

auto DeferredPresentationQueue::drain() -> std::size_t {
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_running.swap(m_pending);
  }
  for (auto& job : m_running) {
    job();
  }
  const std::size_t ran = m_running.size();
  m_running.clear();
  if (ran > 0) {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_drained_jobs += ran;
  }
  return ran;
}

auto DeferredPresentationQueue::pending() const -> std::size_t {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_pending.size();
}

auto DeferredPresentationQueue::drained_jobs() const -> std::uint64_t {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_drained_jobs;
}

} // namespace App::Core
