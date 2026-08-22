#pragma once

#include <cstdint>

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

struct CohortDiagnostics {
  std::uint32_t cohorts_formed{0};
  std::uint32_t cohorts_activated{0};
  std::uint32_t units_in_cohorts{0};
};

class CohortSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

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
