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

    int const current =
        m_full_detail_count.fetch_add(1, std::memory_order_relaxed);
    if (current < budget.max_full_detail_units) {
      return GL::HumanoidLOD::Full;
    }

    m_full_detail_count.fetch_sub(1, std::memory_order_relaxed);
    return GL::HumanoidLOD::Reduced;
  }

  [[nodiscard]] auto request_horse_lod(GL::HorseLOD distance_lod) noexcept
      -> GL::HorseLOD {
    const auto &budget = GraphicsSettings::instance().visibility_budget();
    if (!budget.enabled) {
      return distance_lod;
    }

    if (distance_lod != GL::HorseLOD::Full) {
      return distance_lod;
    }

    int const current =
        m_full_detail_count.fetch_add(1, std::memory_order_relaxed);
    if (current < budget.max_full_detail_units) {
      return GL::HorseLOD::Full;
    }

    m_full_detail_count.fetch_sub(1, std::memory_order_relaxed);
    return GL::HorseLOD::Reduced;
  }

  [[nodiscard]] auto full_detail_count() const noexcept -> int {
    return m_full_detail_count.load(std::memory_order_relaxed);
  }

private:
  VisibilityBudgetTracker() = default;

  std::atomic<int> m_full_detail_count{0};
};

} // namespace Render
