#pragma once

#include <cstdint>

#include "../core/component.h"
#include "../core/entity_id.h"
#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

struct LocalAvoidanceDiagnostics {
  std::uint32_t average_neighbors_checked{0};
  std::uint32_t units_processed{0};
  std::uint32_t units_steered{0};
};

class LocalAvoidanceSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto diagnostics() const -> const LocalAvoidanceDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_separation_radius = 0.15F;

  static constexpr float k_lookahead_seconds = 1.0F;

  static constexpr float k_lean_gain = 0.35F;

  static constexpr float k_min_speed_fraction = 0.35F;

  static constexpr float k_head_on_lane_fraction = 0.25F;

  static constexpr float k_yield_speed_fraction = 0.5F;

private:
  LocalAvoidanceDiagnostics m_diagnostics;
};

} // namespace Game::Systems
