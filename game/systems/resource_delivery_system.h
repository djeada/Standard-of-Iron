#pragma once

#include <cstdint>

#include "../core/system.h"

namespace Game::Systems {

class ResourceDeliverySystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] static auto gather_success_due(std::uint32_t delivery_index,
                                               std::uint32_t hauler_id) -> bool;

private:
  std::uint32_t m_deliveries = 0;
};

} // namespace Game::Systems
