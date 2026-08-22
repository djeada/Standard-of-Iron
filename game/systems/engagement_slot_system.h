#pragma once

#include <cstdint>
#include <vector>

#include "../core/entity_id.h"
#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

struct EngagementSlotDiagnostics {
  std::uint32_t slots_allocated{0};
  std::uint32_t slots_invalidated{0};
  std::uint32_t overflow_redirects{0};
};

class EngagementSlotSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto diagnostics() const -> const EngagementSlotDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_slot_lease_duration = 2.0F;
  static constexpr float k_slot_radius_offset = 1.8F;
  static constexpr std::uint8_t k_max_slots_per_target = 8;

private:
  EngagementSlotDiagnostics m_diagnostics;
  std::vector<Engine::Core::EntityID> m_query_scratch;
};

} // namespace Game::Systems
