#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
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
    m_contact_shadow_count = 0;
    m_contact_shadow_formations.clear();
  }

  [[nodiscard]] auto
  request_humanoid_lod(GL::HumanoidLOD distance_lod) noexcept -> GL::HumanoidLOD {
    const auto& budget = GraphicsSettings::instance().visibility_budget();
    if (!budget.enabled) {
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
    const auto& budget = GraphicsSettings::instance().visibility_budget();
    if (!budget.enabled) {
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

  [[nodiscard]] auto request_contact_shadow(std::uint32_t formation_id,
                                            bool standing_idle) noexcept -> bool {
    const auto& budget = GraphicsSettings::instance().contact_shadow_budget();
    if (!standing_idle || m_contact_shadow_count >= budget.max_casters) {
      return false;
    }
    auto formation =
        std::find_if(m_contact_shadow_formations.begin(),
                     m_contact_shadow_formations.end(),
                     [formation_id](const ContactShadowFormationUsage& usage) {
                       return usage.formation_id == formation_id;
                     });
    if (formation != m_contact_shadow_formations.end() &&
        formation->count >= budget.max_casters_per_formation) {
      return false;
    }
    if (formation == m_contact_shadow_formations.end()) {
      m_contact_shadow_formations.push_back({formation_id, 0});
      formation = std::prev(m_contact_shadow_formations.end());
    }
    ++m_contact_shadow_count;
    ++formation->count;
    return true;
  }

  [[nodiscard]] auto contact_shadow_count() const noexcept -> int {
    return m_contact_shadow_count;
  }

private:
  VisibilityBudgetTracker() = default;

  struct ContactShadowFormationUsage {
    std::uint32_t formation_id{0U};
    int count{0};
  };

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
  int m_contact_shadow_count{0};
  std::vector<ContactShadowFormationUsage> m_contact_shadow_formations;
};

} // namespace Render
