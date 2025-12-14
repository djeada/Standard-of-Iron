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

  m_thread = std::thread(&AIWorker::workerLoop, this);
}

AIWorker::~AIWorker() {
  stop();

  { std::lock_guard<std::mutex> const lock(m_jobMutex); }
  m_jobCondition.notify_all();

  if (m_thread.joinable()) {
    m_thread.join();
  }
}

auto AIWorker::trySubmit(AIJob &&job) -> bool {

  if (m_workerBusy.load(std::memory_order_acquire)) {
    return false;
  }

  {
    std::lock_guard<std::mutex> const lock(m_jobMutex);
    m_pendingJob = std::move(job);
    m_hasPendingJob = true;
  }

  m_workerBusy.store(true, std::memory_order_release);
  m_jobCondition.notify_one();

  return true;
}

void AIWorker::drainResults(std::queue<AIResult> &out) {
  std::lock_guard<std::mutex> const lock(m_resultMutex);

  while (!m_results.empty()) {
    out.push(std::move(m_results.front()));
    m_results.pop();
  }
}

void AIWorker::stop() { m_shouldStop.store(true, std::memory_order_release); }

void AIWorker::workerLoop() {
  while (true) {
    AIJob job;

    {
      std::unique_lock<std::mutex> lock(m_jobMutex);
      m_jobCondition.wait(lock, [this]() {
        return m_shouldStop.load(std::memory_order_acquire) || m_hasPendingJob;
      });

      if (m_shouldStop.load(std::memory_order_acquire) && !m_hasPendingJob) {
        break;
      }

      job = std::move(m_pendingJob);
      m_hasPendingJob = false;
    }

    try {
      AIResult result;
      result.context = job.context;

      Game::Systems::AI::AIReasoner::updateContext(job.snapshot,
                                                   result.context);
      Game::Systems::AI::AIReasoner::updateStateMachine(
          job.snapshot, result.context, job.delta_time);
      Game::Systems::AI::AIReasoner::validateState(result.context);
      Game::Systems::AI::AIExecutor::run(job.snapshot, result.context,
                                         job.delta_time, m_registry,
                                         result.commands);

      {
        std::lock_guard<std::mutex> const lock(m_resultMutex);
        m_results.push(std::move(result));
      }
    } catch (...) {
    }

    m_workerBusy.store(false, std::memory_order_release);
  }

  m_workerBusy.store(false, std::memory_order_release);
}

} // namespace Game::Systems::AI
