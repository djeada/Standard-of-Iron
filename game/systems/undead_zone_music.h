#pragma once

#include <set>
#include <vector>

#include "undead_zone_runtime.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class OwnerRegistry;

class UndeadZoneMusic {
public:
  explicit UndeadZoneMusic(const OwnerRegistry& owners);

  void update(Engine::Core::World& world,
              const std::vector<UndeadRuntimeZone>& zones,
              float delta_time);

private:
  [[nodiscard]] auto owners_in_woken_zones(
      Engine::Core::World& world,
      const std::vector<UndeadRuntimeZone>& zones) const -> std::set<int>;

  const OwnerRegistry& m_owners;
  std::set<int> m_playing;
  float m_poll = 0.0F;
};

} // namespace Game::Systems
