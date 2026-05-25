#pragma once

#include "../core/system.h"
#include "../core/world.h"

namespace Game::Systems {

/// Diagnostics for engagement slot allocation.
struct EngagementSlotDiagnostics {
  std::uint32_t slots_allocated{0};
  std::uint32_t slots_invalidated{0};
  std::uint32_t overflow_redirects{0};
};

/// Melee engagement slot allocator (Workstream 3).
///
/// Assigns engagement slots around targets so melee attackers spread around
/// the target perimeter instead of all stacking on the same point.
class EngagementSlotSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto diagnostics() const -> const EngagementSlotDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_slot_lease_duration = 2.0F;
  static constexpr float k_slot_radius_offset = 1.8F;
  static constexpr std::uint8_t k_max_slots_per_target = 8;

private:
  EngagementSlotDiagnostics m_diagnostics;
};

} // namespace Game::Systems
