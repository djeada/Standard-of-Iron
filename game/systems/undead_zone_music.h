#pragma once

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
  [[nodiscard]] auto local_player_inside(Engine::Core::World& world,
                                         const UndeadRuntimeZone& zone) const -> bool;
  [[nodiscard]] auto
  inside_a_woken_zone(Engine::Core::World& world,
                      const std::vector<UndeadRuntimeZone>& zones) const -> bool;

  const OwnerRegistry& m_owners;
  bool m_playing = false;
  float m_poll = 0.0F;
};

} // namespace Game::Systems
