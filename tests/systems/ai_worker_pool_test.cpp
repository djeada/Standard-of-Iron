#include <queue>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "game/systems/ai_system/ai_behavior.h"
#include "game/systems/ai_system/ai_behavior_registry.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/systems/ai_system/ai_worker.h"

namespace {

using Game::Systems::AI::AIBehavior;
using Game::Systems::AI::AIBehaviorRegistry;
using Game::Systems::AI::AIContext;
using Game::Systems::AI::AIJob;
using Game::Systems::AI::AIResult;
using Game::Systems::AI::AIWorker;
using Game::Systems::AI::AIWorkerPool;
using Game::Systems::AI::BehaviorPriority;

class CountingBehavior : public AIBehavior {
public:
  void execute(const Game::Systems::AI::AISnapshot&,
               AIContext&,
               float,
               std::vector<Game::Systems::AI::AICommand>&) override {
    m_executions.fetch_add(1, std::memory_order_acq_rel);
  }

  [[nodiscard]] auto should_execute(const Game::Systems::AI::AISnapshot&,
                                    const AIContext&) const -> bool override {
    return true;
  }

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Normal;
  }

  [[nodiscard]] auto executions() const -> int {
    return m_executions.load(std::memory_order_acquire);
  }

private:
  std::atomic<int> m_executions{0};
};

class BlockingBehavior : public AIBehavior {
public:
  void execute(const Game::Systems::AI::AISnapshot&,
               AIContext&,
               float,
               std::vector<Game::Systems::AI::AICommand>&) override {
    m_entered.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this]() { return m_released; });
  }

  [[nodiscard]] auto should_execute(const Game::Systems::AI::AISnapshot&,
                                    const AIContext&) const -> bool override {
    return true;
  }

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Normal;
  }

  [[nodiscard]] auto entered() const -> bool {
    return m_entered.load(std::memory_order_acquire);
  }

  void release() {
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_released = true;
    }
    m_condition.notify_all();
  }

private:
  std::atomic<bool> m_entered{false};
  std::mutex m_mutex;
  std::condition_variable m_condition;
  bool m_released = false;
};

auto drain_count(AIWorker& worker) -> std::size_t {
  std::queue<AIResult> results;
  worker.drain_results(results);
  return results.size();
}

} // namespace

TEST(AIWorkerPoolTest, ThreadCountIsBoundedAndNeverExceedsTheOpponentCount) {
  EXPECT_EQ(AIWorkerPool::default_thread_count(0), 0U);
  EXPECT_EQ(AIWorkerPool::default_thread_count(1), 1U);
  EXPECT_LE(AIWorkerPool::default_thread_count(7), 3U);
  EXPECT_GE(AIWorkerPool::default_thread_count(7), 1U);
}

TEST(AIWorkerPoolTest, OneThreadServesEveryOpponentSlot) {
  AIWorkerPool pool(1);

  constexpr std::size_t k_opponents = 5;
  std::vector<std::unique_ptr<AIBehaviorRegistry>> registries;
  std::vector<CountingBehavior*> behaviors;
  std::vector<std::unique_ptr<AIWorker>> workers;

  for (std::size_t index = 0; index < k_opponents; ++index) {
    auto registry = std::make_unique<AIBehaviorRegistry>();
    auto behavior = std::make_unique<CountingBehavior>();
    behaviors.push_back(behavior.get());
    registry->register_behavior(std::move(behavior));
    workers.push_back(std::make_unique<AIWorker>(pool, *registry));
    registries.push_back(std::move(registry));
  }

  for (auto& worker : workers) {
    ASSERT_TRUE(worker->try_submit(AIJob{}));
  }

  std::size_t completed = 0;
  for (auto& worker : workers) {
    ASSERT_TRUE(worker->wait_idle(std::chrono::seconds(5)));
    completed += drain_count(*worker);
  }

  EXPECT_EQ(completed, k_opponents);
  for (const auto* behavior : behaviors) {
    EXPECT_EQ(behavior->executions(), 1);
  }
}

TEST(AIWorkerPoolTest, ABusySlotRefusesASecondJob) {
  AIWorkerPool pool(1);
  AIBehaviorRegistry registry;
  auto behavior = std::make_unique<BlockingBehavior>();
  auto* blocking = behavior.get();
  registry.register_behavior(std::move(behavior));

  AIWorker worker(pool, registry);
  ASSERT_TRUE(worker.try_submit(AIJob{}));
  while (!blocking->entered()) {
    std::this_thread::yield();
  }

  EXPECT_FALSE(worker.try_submit(AIJob{}));

  blocking->release();
  EXPECT_TRUE(worker.wait_idle(std::chrono::seconds(5)));
}

TEST(AIWorkerPoolTest, AStalledDecisionGivesTheWaitBackWithinItsBudget) {
  AIWorkerPool pool(1);
  AIBehaviorRegistry registry;
  auto behavior = std::make_unique<BlockingBehavior>();
  auto* blocking = behavior.get();
  registry.register_behavior(std::move(behavior));

  AIWorker worker(pool, registry);
  ASSERT_TRUE(worker.try_submit(AIJob{}));
  while (!blocking->entered()) {
    std::this_thread::yield();
  }

  const auto started = std::chrono::steady_clock::now();
  const bool finished = worker.wait_idle(std::chrono::milliseconds(20));
  const auto waited = std::chrono::steady_clock::now() - started;

  EXPECT_FALSE(finished);
  EXPECT_LT(waited, std::chrono::seconds(2));
  EXPECT_EQ(drain_count(worker), 0U);

  blocking->release();
  EXPECT_TRUE(worker.wait_idle(std::chrono::seconds(5)));
  EXPECT_EQ(drain_count(worker), 1U);
}

TEST(AIWorkerPoolTest, ASlotDestroyedWhileQueuedDoesNotHang) {
  AIWorkerPool pool(1);

  AIBehaviorRegistry blocking_registry;
  auto behavior = std::make_unique<BlockingBehavior>();
  auto* blocking = behavior.get();
  blocking_registry.register_behavior(std::move(behavior));
  AIWorker occupying(pool, blocking_registry);
  ASSERT_TRUE(occupying.try_submit(AIJob{}));
  while (!blocking->entered()) {
    std::this_thread::yield();
  }

  {
    AIBehaviorRegistry queued_registry;
    queued_registry.register_behavior(std::make_unique<CountingBehavior>());
    AIWorker queued(pool, queued_registry);
    ASSERT_TRUE(queued.try_submit(AIJob{}));
  }

  blocking->release();
  EXPECT_TRUE(occupying.wait_idle(std::chrono::seconds(5)));
}

TEST(AIWorkerPoolTest, StoppingThePoolReleasesSlotsWaitingForAThread) {
  auto pool = std::make_unique<AIWorkerPool>(1);

  AIBehaviorRegistry blocking_registry;
  auto behavior = std::make_unique<BlockingBehavior>();
  auto* blocking = behavior.get();
  blocking_registry.register_behavior(std::move(behavior));
  AIWorker occupying(*pool, blocking_registry);
  ASSERT_TRUE(occupying.try_submit(AIJob{}));
  while (!blocking->entered()) {
    std::this_thread::yield();
  }

  AIBehaviorRegistry queued_registry;
  queued_registry.register_behavior(std::make_unique<CountingBehavior>());
  AIWorker queued(*pool, queued_registry);
  ASSERT_TRUE(queued.try_submit(AIJob{}));

  blocking->release();
  pool->stop();

  EXPECT_TRUE(queued.wait_idle(std::chrono::seconds(5)));
  EXPECT_TRUE(occupying.wait_idle(std::chrono::seconds(5)));
}
