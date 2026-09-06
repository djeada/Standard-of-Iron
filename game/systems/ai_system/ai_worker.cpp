#include "ai_worker.h"

#include <QDebug>
#include <queue>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

#include "systems/ai_system/ai_behavior_registry.h"
#include "systems/ai_system/ai_executor.h"
#include "systems/ai_system/ai_reasoner.h"
#include "systems/ai_system/ai_stall_recovery.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr std::size_t k_max_pool_threads = 3;

} // namespace

auto AIWorkerPool::default_thread_count(std::size_t ai_count) -> std::size_t {
  if (ai_count == 0) {
    return 0;
  }
  const unsigned int hardware = std::max(1U, std::thread::hardware_concurrency());
  const std::size_t share = static_cast<std::size_t>(hardware) / 4U;
  return std::clamp<std::size_t>(share, 1U, std::min(ai_count, k_max_pool_threads));
}

AIWorkerPool::AIWorkerPool(std::size_t thread_count) {
  m_threads.reserve(thread_count);
  for (std::size_t index = 0; index < thread_count; ++index) {
    m_threads.emplace_back(&AIWorkerPool::thread_loop, this);
  }
}

AIWorkerPool::~AIWorkerPool() {
  stop();
}

void AIWorkerPool::stop() {
  std::deque<AIWorker*> abandoned;
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_stopping = true;
    abandoned.swap(m_queue);
  }
  m_condition.notify_all();

  for (AIWorker* worker : abandoned) {
    worker->discard_pending_job();
  }

  for (auto& thread : m_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  m_threads.clear();
}

void AIWorkerPool::enqueue(AIWorker& worker) {
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) {
      return;
    }
    m_queue.push_back(&worker);
  }
  m_condition.notify_one();
}

auto AIWorkerPool::cancel(AIWorker& worker) -> bool {
  const std::lock_guard<std::mutex> lock(m_mutex);
  const auto removed = std::remove(m_queue.begin(), m_queue.end(), &worker);
  if (removed == m_queue.end()) {
    return false;
  }
  m_queue.erase(removed, m_queue.end());
  return true;
}

void AIWorkerPool::thread_loop() {
  while (true) {
    AIWorker* worker = nullptr;

    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_condition.wait(lock, [this]() { return m_stopping || !m_queue.empty(); });

      if (m_stopping) {
        break;
      }

      worker = m_queue.front();
      m_queue.pop_front();
    }

    worker->run_pending_job();
  }
}

AIWorker::AIWorker(AIWorkerPool& pool, AIBehaviorRegistry& registry)
    : m_pool(pool)
    , m_registry(registry) {
}

AIWorker::~AIWorker() {
  stop();
  if (m_pool.cancel(*this)) {
    discard_pending_job();
  }
  wait_idle();
}

auto AIWorker::try_submit(AIJob&& job) -> bool {

  if (m_should_stop.load(std::memory_order_acquire)) {
    return false;
  }

  if (m_worker_busy.load(std::memory_order_acquire)) {
    return false;
  }

  {
    const std::lock_guard<std::mutex> lock(m_job_mutex);
    m_pending_job = std::move(job);
    m_has_pending_job = true;
  }

  m_worker_busy.store(true, std::memory_order_release);
  m_pool.enqueue(*this);

  return true;
}

void AIWorker::drain_results(std::queue<AIResult>& out) {
  const std::lock_guard<std::mutex> lock(m_result_mutex);

  while (!m_results.empty()) {
    out.push(std::move(m_results.front()));
    m_results.pop();
  }
}

auto AIWorker::wait_idle(std::chrono::microseconds budget) -> bool {
  std::unique_lock<std::mutex> lock(m_result_mutex);
  return m_idle_condition.wait_for(lock, budget, [this]() {
    return !m_worker_busy.load(std::memory_order_acquire);
  });
}

void AIWorker::wait_idle() {
  std::unique_lock<std::mutex> lock(m_result_mutex);
  m_idle_condition.wait(
      lock, [this]() { return !m_worker_busy.load(std::memory_order_acquire); });
}

void AIWorker::stop() {
  m_should_stop.store(true, std::memory_order_release);
}

void AIWorker::discard_pending_job() {
  {
    const std::lock_guard<std::mutex> lock(m_job_mutex);
    m_pending_job = AIJob{};
    m_has_pending_job = false;
  }
  {
    const std::lock_guard<std::mutex> lock(m_result_mutex);
    m_worker_busy.store(false, std::memory_order_release);
  }
  m_idle_condition.notify_all();
}

void AIWorker::run_pending_job() {
  AIJob job;

  {
    const std::lock_guard<std::mutex> lock(m_job_mutex);
    if (!m_has_pending_job) {
      m_worker_busy.store(false, std::memory_order_release);
      m_idle_condition.notify_all();
      return;
    }
    job = std::move(m_pending_job);
    m_pending_job = AIJob{};
    m_has_pending_job = false;
  }

  try {
    AIResult result;
    result.context = job.context;

    AIReasoner::update_context(job.snapshot, result.context);
    AIReasoner::update_state_machine(job.snapshot, result.context, job.delta_time);
    AIReasoner::validate_state(result.context);
    update_stall_recovery(job.snapshot, result.context, result.commands);
    AIExecutor::run(
        job.snapshot, result.context, job.delta_time, m_registry, result.commands);
    result.context.nation = nullptr;

    {
      const std::lock_guard<std::mutex> lock(m_result_mutex);
      m_results.push(std::move(result));
    }
  } catch (const std::exception& ex) {
    qWarning() << "AIWorker job failed:" << ex.what();
  } catch (...) {
    qWarning() << "AIWorker job failed with an unknown exception";
  }

  {
    const std::lock_guard<std::mutex> lock(m_result_mutex);
    m_worker_busy.store(false, std::memory_order_release);
  }
  m_idle_condition.notify_all();
}

} // namespace Game::Systems::AI
