#pragma once

#include <queue>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "ai_behavior_registry.h"
#include "ai_types.h"

namespace Game::Systems::AI {

class AIWorker;

class AIWorkerPool {
public:
  [[nodiscard]] static auto default_thread_count(std::size_t ai_count) -> std::size_t;

  explicit AIWorkerPool(std::size_t thread_count);
  ~AIWorkerPool();

  AIWorkerPool(const AIWorkerPool&) = delete;
  auto operator=(const AIWorkerPool&) -> AIWorkerPool& = delete;
  AIWorkerPool(AIWorkerPool&&) = delete;
  auto operator=(AIWorkerPool&&) -> AIWorkerPool& = delete;

  void stop();

  [[nodiscard]] auto thread_count() const -> std::size_t { return m_threads.size(); }

private:
  friend class AIWorker;

  void enqueue(AIWorker& worker);
  auto cancel(AIWorker& worker) -> bool;
  void thread_loop();

  std::vector<std::thread> m_threads;
  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::deque<AIWorker*> m_queue;
  bool m_stopping = false;
};

class AIWorker {
public:
  AIWorker(AIWorkerPool& pool, AIBehaviorRegistry& registry);

  ~AIWorker();

  AIWorker(const AIWorker&) = delete;
  auto operator=(const AIWorker&) -> AIWorker& = delete;
  AIWorker(AIWorker&&) = delete;
  auto operator=(AIWorker&&) -> AIWorker& = delete;

  auto try_submit(AIJob&& job) -> bool;

  void drain_results(std::queue<AIResult>& out);

  auto wait_idle(std::chrono::microseconds budget) -> bool;

  void wait_idle();

  void stop();

  [[nodiscard]] auto busy() const -> bool {
    return m_worker_busy.load(std::memory_order_acquire);
  }

private:
  friend class AIWorkerPool;

  void run_pending_job();
  void discard_pending_job();

  AIWorkerPool& m_pool;
  AIBehaviorRegistry& m_registry;

  std::atomic<bool> m_should_stop{false};
  std::atomic<bool> m_worker_busy{false};

  std::mutex m_job_mutex;
  bool m_has_pending_job = false;
  AIJob m_pending_job;

  std::mutex m_result_mutex;
  std::queue<AIResult> m_results;
  std::condition_variable m_idle_condition;
};

} // namespace Game::Systems::AI
