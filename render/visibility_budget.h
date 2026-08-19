#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <vector>

#include "entity/registry.h"
#include "graphics_settings.h"

namespace Render {

class VisibilityBudgetTracker {
public:
  static auto instance() noexcept -> VisibilityBudgetTracker& {
    static VisibilityBudgetTracker inst;
    return inst;
  }

  void reset_frame() noexcept {
    m_full_detail_count.store(0, std::memory_order_relaxed);
    std::lock_guard const lock(m_contact_shadow_mutex);
    m_contact_shadow_count = 0;
  }

  [[nodiscard]] auto
  request_humanoid_lod(GL::HumanoidLOD distance_lod) noexcept -> GL::HumanoidLOD {
    const auto& budget = GraphicsSettings::instance().creature_lod();
    if (!budget.visibility_budget) {
      return distance_lod;
    }

    if (distance_lod != GL::HumanoidLOD::Full) {
      return distance_lod;
    }

    if (try_consume_budget(budget.max_full_detail_units)) {
      return GL::HumanoidLOD::Full;
    }
    return GL::HumanoidLOD::Minimal;
  }

  [[nodiscard]] auto
  request_horse_lod(GL::HorseLOD distance_lod) noexcept -> GL::HorseLOD {
    const auto& budget = GraphicsSettings::instance().creature_lod();
    if (!budget.visibility_budget) {
      return distance_lod;
    }

    if (distance_lod != GL::HorseLOD::Full) {
      return distance_lod;
    }

    if (try_consume_budget(budget.max_full_detail_units)) {
      return GL::HorseLOD::Full;
    }
    return GL::HorseLOD::Minimal;
  }

  [[nodiscard]] auto full_detail_count() const noexcept -> int {
    return m_full_detail_count.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto request_contact_shadow() noexcept -> bool {
    const auto& budget = GraphicsSettings::instance().contact_shadow_budget();
    std::lock_guard const lock(m_contact_shadow_mutex);
    if (m_contact_shadow_count >= budget.max_casters) {
      return false;
    }
    ++m_contact_shadow_count;
    return true;
  }

  [[nodiscard]] auto contact_shadow_count() const noexcept -> int {
    std::lock_guard const lock(m_contact_shadow_mutex);
    return m_contact_shadow_count;
  }

private:
  VisibilityBudgetTracker() = default;

  [[nodiscard]] auto try_consume_budget(int max_units) noexcept -> bool {
    int current = m_full_detail_count.load(std::memory_order_relaxed);
    while (current < max_units) {
      if (m_full_detail_count.compare_exchange_weak(current,
                                                    current + 1,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  std::atomic<int> m_full_detail_count{0};
  mutable std::mutex m_contact_shadow_mutex;
  int m_contact_shadow_count{0};
};

} // namespace Render
