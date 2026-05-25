#pragma once

#include "../core/system.h"
#include "../core/world.h"

namespace Game::Systems {

/// Diagnostics for cohort system.
struct CohortDiagnostics {
  std::uint32_t cohorts_formed{0};
  std::uint32_t cohorts_activated{0};
  std::uint32_t units_in_cohorts{0};
};

/// Local AI cohort system (Workstream 6).
///
/// Clusters idle/guarding AI units by local radius so that when one unit
/// detects an enemy, its local cohort responds together instead of just the
/// nearest single unit or the entire army.
class CohortSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto diagnostics() const -> const CohortDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_cohort_radius = 8.0F;
  static constexpr std::uint32_t k_max_cohort_size = 12;
  static constexpr float k_reform_interval = 3.0F;

private:
  CohortDiagnostics m_diagnostics;
  float m_reform_timer{0.0F};
  std::uint32_t m_next_cohort_id{1};
};

} // namespace Game::Systems
