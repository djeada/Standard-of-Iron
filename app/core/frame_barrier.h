#pragma once

#include <atomic>
#include <chrono>
#include <thread>

namespace App::Core {

class FrameBarrier {
public:
  using Clock = std::chrono::steady_clock;

  enum class FreezeResult : int {
    Acquired = 0,
    RenderBusy,
    SimulationBusy
  };

  [[nodiscard]] auto try_begin_render() -> bool {
    m_render_active.store(true);
    if (m_freeze_depth.load() > 0) {
      m_render_active.store(false);
      return false;
    }
    return true;
  }

  void end_render() { m_render_active.store(false); }

  [[nodiscard]] auto try_begin_simulation() -> bool {
    m_simulation_active.store(true);
    if (m_freeze_depth.load() > 0) {
      m_simulation_active.store(false);
      return false;
    }
    return true;
  }

  void end_simulation() { m_simulation_active.store(false); }

  [[nodiscard]] auto try_freeze(Clock::duration budget,
                                Clock::duration poll) -> FreezeResult {
    m_freeze_depth.fetch_add(1);
    const auto deadline = Clock::now() + budget;
    while (m_render_active.load() || m_simulation_active.load()) {
      if (Clock::now() >= deadline) {
        const bool render_busy = m_render_active.load();
        m_freeze_depth.fetch_sub(1);
        m_refusals.fetch_add(1, std::memory_order_acq_rel);
        return render_busy ? FreezeResult::RenderBusy : FreezeResult::SimulationBusy;
      }
      std::this_thread::sleep_for(poll);
    }
    return FreezeResult::Acquired;
  }

  void release_freeze() { m_freeze_depth.fetch_sub(1); }

  [[nodiscard]] auto frozen() const -> bool {
    return m_freeze_depth.load(std::memory_order_acquire) > 0;
  }
  [[nodiscard]] auto render_active() const -> bool {
    return m_render_active.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto simulation_active() const -> bool {
    return m_simulation_active.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto refusals() const -> int {
    return m_refusals.load(std::memory_order_acquire);
  }

private:
  std::atomic<int> m_freeze_depth{0};
  std::atomic<int> m_refusals{0};
  std::atomic<bool> m_render_active{false};
  std::atomic<bool> m_simulation_active{false};
};

} // namespace App::Core
