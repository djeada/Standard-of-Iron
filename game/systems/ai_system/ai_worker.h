#pragma once

#include "ai_executor.h"
#include "ai_reasoner.h"
#include "ai_types.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace Game::Systems::AI {

class AIWorker {
public:
  AIWorker(AIReasoner &reasoner, AIExecutor &executor,
           AIBehaviorRegistry &registry);

  ~AIWorker();

  AIWorker(const AIWorker &) = delete;
  auto operator=(const AIWorker &) -> AIWorker & = delete;
  AIWorker(AIWorker &&) = delete;
  auto operator=(AIWorker &&) -> AIWorker & = delete;

  auto try_submit(AIJob &&job) -> bool;

  void drain_results(std::queue<AIResult> &out);

  auto busy() const noexcept -> bool {
    return m_worker_busy.load(std::memory_order_acquire);
  }

  void stop();

private:
  void worker_loop();

  AIReasoner &m_reasoner;
  AIExecutor &m_executor;
  AIBehaviorRegistry &m_registry;

  std::thread m_thread;
  std::atomic<bool> m_should_stop{false};
  std::atomic<bool> m_worker_busy{false};

  std::mutex m_job_mutex;
  std::condition_variable m_job_condition;
  bool m_has_pending_job = false;
  AIJob m_pending_job;

  std::mutex m_result_mutex;
  std::queue<AIResult> m_results;
};

} // namespace Game::Systems::AI
