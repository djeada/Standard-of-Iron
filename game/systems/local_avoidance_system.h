#pragma once

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"

namespace Game::Systems {

struct LocalAvoidanceDiagnostics {
  float spatial_hash_build_ms{0.0F};
  std::uint32_t average_neighbors_checked{0};
  std::uint32_t units_processed{0};
  std::uint32_t overlaps_detected{0};
};

class LocalAvoidanceSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto diagnostics() const -> const LocalAvoidanceDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_default_cell_size = 4.0F;
  static constexpr float k_separation_radius = 2.0F;
  static constexpr float k_max_correction_per_tick = 0.3F;
  static constexpr float k_separation_strength = 1.5F;

private:
  LocalAvoidanceDiagnostics m_diagnostics;
};

} // namespace Game::Systems
