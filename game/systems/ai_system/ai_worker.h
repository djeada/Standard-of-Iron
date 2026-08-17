#pragma once

#include <queue>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "ai_behavior_registry.h"
#include "ai_types.h"

namespace Game::Systems::AI {

class AIWorker {
public:
  explicit AIWorker(AIBehaviorRegistry& registry);

  ~AIWorker();

  AIWorker(const AIWorker&) = delete;
  auto operator=(const AIWorker&) -> AIWorker& = delete;
  AIWorker(AIWorker&&) = delete;
  auto operator=(AIWorker&&) -> AIWorker& = delete;

  auto try_submit(AIJob&& job) -> bool;

  void drain_results(std::queue<AIResult>& out);

  void wait_idle();

  void stop();

private:
  void worker_loop();

  AIBehaviorRegistry& m_registry;

  std::thread m_thread;
  std::atomic<bool> m_should_stop{false};
  std::atomic<bool> m_worker_busy{false};

  std::mutex m_job_mutex;
  std::condition_variable m_job_condition;
  bool m_has_pending_job = false;
  AIJob m_pending_job;

  std::mutex m_result_mutex;
  std::queue<AIResult> m_results;
  std::condition_variable m_idle_condition;
};

} // namespace Game::Systems::AI
