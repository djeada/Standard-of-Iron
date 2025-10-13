#include "ai_worker.h"

namespace Game::Systems::AI {

AIWorker::AIWorker(AIReasoner &reasoner, AIExecutor &executor,
                   AIBehaviorRegistry &registry)
    : m_reasoner(reasoner), m_executor(executor), m_registry(registry) {

  m_thread = std::thread(&AIWorker::workerLoop, this);
}

AIWorker::~AIWorker() {
  stop();

  { std::lock_guard<std::mutex> lock(m_jobMutex); }
  m_jobCondition.notify_all();

  if (m_thread.joinable()) {
    m_thread.join();
  }
}

bool AIWorker::trySubmit(AIJob &&job) {

  if (m_workerBusy.load(std::memory_order_acquire)) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(m_jobMutex);
    m_pendingJob = std::move(job);
    m_hasPendingJob = true;
  }

  m_workerBusy.store(true, std::memory_order_release);
  m_jobCondition.notify_one();

  return true;
}

void AIWorker::drainResults(std::queue<AIResult> &out) {
  std::lock_guard<std::mutex> lock(m_resultMutex);

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

      m_reasoner.updateContext(job.snapshot, result.context);
      m_reasoner.updateStateMachine(result.context, job.deltaTime);
      m_executor.run(job.snapshot, result.context, job.deltaTime, m_registry,
                     result.commands);

      {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_results.push(std::move(result));
      }
    } catch (...) {
    }

    m_workerBusy.store(false, std::memory_order_release);
  }

  m_workerBusy.store(false, std::memory_order_release);
}

} // namespace Game::Systems::AI
