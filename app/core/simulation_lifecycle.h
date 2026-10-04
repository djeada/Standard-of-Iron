#pragma once

#include <QThread>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "app/core/frame_barrier.h"

namespace App::Core {

struct FrameLockStats {
  std::atomic<std::uint64_t> uncontended{0};
  std::atomic<std::uint64_t> contended{0};
  std::atomic<std::uint64_t> waited_us{0};
  std::atomic<std::uint64_t> longest_wait_us{0};
  std::atomic<std::uint64_t> deferred_presentations{0};
  std::atomic<std::uint64_t> forced_presentation_waits{0};
  std::atomic<std::uint64_t> simulation_handoff_yields{0};
  std::atomic<std::uint64_t> try_lock_skips{0};
};

class SimulationLifecycle {
public:
  using TickBody = std::function<void(float dt)>;
  using UnlockedStage = std::function<void()>;

  SimulationLifecycle() = default;
  ~SimulationLifecycle();
  SimulationLifecycle(const SimulationLifecycle&) = delete;
  auto operator=(const SimulationLifecycle&) -> SimulationLifecycle& = delete;
  SimulationLifecycle(SimulationLifecycle&&) = delete;
  auto operator=(SimulationLifecycle&&) -> SimulationLifecycle& = delete;

  void start(TickBody body, UnlockedStage after_unlock = {});
  void stop();
  [[nodiscard]] auto running() const -> bool {
    return m_running.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto take_tick_us() -> std::uint64_t {
    return m_tick_us.exchange(0, std::memory_order_acq_rel);
  }

  [[nodiscard]] auto lock_frame() -> std::unique_lock<std::recursive_mutex>;
  [[nodiscard]] auto try_lock_frame() -> std::unique_lock<std::recursive_mutex>;
  [[nodiscard]] auto frame_mutex() const -> std::recursive_mutex& {
    return m_frame_mutex;
  }
  [[nodiscard]] auto stats() const -> const FrameLockStats& { return m_stats; }

  [[nodiscard]] auto barrier() -> FrameBarrier& { return m_barrier; }
  [[nodiscard]] auto barrier() const -> const FrameBarrier& { return m_barrier; }

private:
  void run();

  static constexpr int k_handoff_yields = 64;

  FrameBarrier m_barrier;
  std::atomic<bool> m_running{false};
  std::atomic<std::uint64_t> m_tick_us{0};
  std::atomic<int> m_lock_waiters{0};
  mutable std::recursive_mutex m_frame_mutex;
  mutable FrameLockStats m_stats;
  TickBody m_body;
  UnlockedStage m_after_unlock;
  std::unique_ptr<QThread> m_thread;
};

} // namespace App::Core
