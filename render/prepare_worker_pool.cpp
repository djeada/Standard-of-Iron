#include "prepare_worker_pool.h"

#include <algorithm>

namespace Render {

PrepareWorkerPool::PrepareWorkerPool() {
  unsigned const hardware = std::thread::hardware_concurrency();
  std::size_t const workers =
      hardware > 1U ? std::min<std::size_t>(hardware - 1U, 3U) : 0U;
  m_workers.reserve(workers);
  for (std::size_t i = 0; i < workers; ++i) {
    m_workers.emplace_back([this] { worker_loop(); });
  }
}

PrepareWorkerPool::~PrepareWorkerPool() {
  {
    std::lock_guard const lock(m_mutex);
    m_stop = true;
  }
  m_start.notify_all();
  for (auto& worker : m_workers) {
    worker.join();
  }
}

void PrepareWorkerPool::drain() {
  for (;;) {
    std::size_t const index = m_next.fetch_add(1, std::memory_order_relaxed);
    if (index >= m_job_count) {
      return;
    }
    (*m_job)(index);
  }
}

void PrepareWorkerPool::worker_loop() {
  std::size_t seen_generation = 0;
  for (;;) {
    {
      std::unique_lock lock(m_mutex);
      m_start.wait(lock, [&] { return m_stop || m_generation != seen_generation; });
      if (m_stop) {
        return;
      }
      seen_generation = m_generation;
    }
    drain();
    {
      std::lock_guard const lock(m_mutex);
      ++m_idle_workers;
    }
    m_done.notify_one();
  }
}

void PrepareWorkerPool::run(std::size_t job_count, const Job& job) {
  if (job_count == 0) {
    return;
  }
  if (m_workers.empty()) {
    for (std::size_t i = 0; i < job_count; ++i) {
      job(i);
    }
    return;
  }
  {
    std::lock_guard const lock(m_mutex);
    m_job = &job;
    m_job_count = job_count;
    m_next.store(0, std::memory_order_relaxed);
    m_idle_workers = 0;
    ++m_generation;
  }
  m_start.notify_all();
  drain();
  std::unique_lock lock(m_mutex);
  m_done.wait(lock, [&] { return m_idle_workers == m_workers.size(); });
  m_job = nullptr;
  m_job_count = 0;
}

} // namespace Render
