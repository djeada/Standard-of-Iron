#include "ai_worker.h"
#include "systems/ai_system/ai_behavior_registry.h"
#include "systems/ai_system/ai_executor.h"
#include "systems/ai_system/ai_reasoner.h"
#include "systems/ai_system/ai_types.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <utility>

namespace Game::Systems::AI {

AIWorker::AIWorker(AIReasoner &reasoner, AIExecutor &executor,
                   AIBehaviorRegistry &registry)
    : m_reasoner(reasoner), m_executor(executor), m_registry(registry) {

  m_thread = std::thread(&AIWorker::worker_loop, this);
}

AIWorker::~AIWorker() {
  stop();

  { std::lock_guard<std::mutex> const lock(m_job_mutex); }
  m_job_condition.notify_all();

  if (m_thread.joinable()) {
    m_thread.join();
  }
}

auto AIWorker::try_submit(AIJob &&job) -> bool {

  if (m_worker_busy.load(std::memory_order_acquire)) {
    return false;
  }

  {
    std::lock_guard<std::mutex> const lock(m_job_mutex);
    m_pending_job = std::move(job);
    m_has_pending_job = true;
  }

  m_worker_busy.store(true, std::memory_order_release);
  m_job_condition.notify_one();

  return true;
}

void AIWorker::drain_results(std::queue<AIResult> &out) {
  std::lock_guard<std::mutex> const lock(m_result_mutex);

  while (!m_results.empty()) {
    out.push(std::move(m_results.front()));
    m_results.pop();
  }
}

void AIWorker::stop() { m_should_stop.store(true, std::memory_order_release); }

void AIWorker::worker_loop() {
  while (true) {
    AIJob job;

    {
      std::unique_lock<std::mutex> lock(m_job_mutex);
      m_job_condition.wait(lock, [this]() {
        return m_should_stop.load(std::memory_order_acquire) ||
               m_has_pending_job;
      });

      if (m_should_stop.load(std::memory_order_acquire) && !m_has_pending_job) {
        break;
      }

      job = std::move(m_pending_job);
      m_has_pending_job = false;
    }

    try {
      AIResult result;
      result.context = job.context;

      Game::Systems::AI::AIReasoner::update_context(job.snapshot,
                                                    result.context);
      Game::Systems::AI::AIReasoner::update_state_machine(
          job.snapshot, result.context, job.delta_time);
      Game::Systems::AI::AIReasoner::validate_state(result.context);
      Game::Systems::AI::AIExecutor::run(job.snapshot, result.context,
                                         job.delta_time, m_registry,
                                         result.commands);

      {
        std::lock_guard<std::mutex> const lock(m_result_mutex);
        m_results.push(std::move(result));
      }
    } catch (...) {
    }

    m_worker_busy.store(false, std::memory_order_release);
  }

  m_worker_busy.store(false, std::memory_order_release);
}

} // namespace Game::Systems::AI
