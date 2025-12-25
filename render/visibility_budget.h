#pragma once

#include "entity/registry.h"
#include "graphics_settings.h"
#include <atomic>
#include <cstdint>

namespace Render {

class VisibilityBudgetTracker {
public:
  static auto instance() noexcept -> VisibilityBudgetTracker & {
    static VisibilityBudgetTracker inst;
    return inst;
  }

  void reset_frame() noexcept {
    m_full_detail_count.store(0, std::memory_order_relaxed);
  }

  [[nodiscard]] auto request_humanoid_lod(GL::HumanoidLOD distance_lod) noexcept
      -> GL::HumanoidLOD {
    const auto &budget = GraphicsSettings::instance().visibility_budget();
    if (!budget.enabled) {
      return distance_lod;
    }

    if (distance_lod != GL::HumanoidLOD::Full) {
      return distance_lod;
    }

    if (try_consume_budget(budget.max_full_detail_units)) {
      return GL::HumanoidLOD::Full;
    }
    return GL::HumanoidLOD::Reduced;
  }

  [[nodiscard]] auto
  request_horse_lod(GL::HorseLOD distance_lod) noexcept -> GL::HorseLOD {
    const auto &budget = GraphicsSettings::instance().visibility_budget();
    if (!budget.enabled) {
      return distance_lod;
    }

    if (distance_lod != GL::HorseLOD::Full) {
      return distance_lod;
    }

    if (try_consume_budget(budget.max_full_detail_units)) {
      return GL::HorseLOD::Full;
    }
    return GL::HorseLOD::Reduced;
  }

  [[nodiscard]] auto full_detail_count() const noexcept -> int {
    return m_full_detail_count.load(std::memory_order_relaxed);
  }

private:
  VisibilityBudgetTracker() = default;

  [[nodiscard]] auto try_consume_budget(int max_units) noexcept -> bool {
    int current = m_full_detail_count.load(std::memory_order_relaxed);
    while (current < max_units) {
      if (m_full_detail_count.compare_exchange_weak(
              current, current + 1, std::memory_order_relaxed,
              std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  std::atomic<int> m_full_detail_count{0};
};

} 
