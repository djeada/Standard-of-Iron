#pragma once

#include <span>
#include <vector>

#include "core/entity.h"
#include "core/system.h"
#include "units/spawn_type.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class CaptureSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  static void transfer_barrack_ownership(Engine::Core::World* world,
                                         Engine::Core::Entity* barrack,
                                         int new_owner_id);

private:
  struct CaptureTroop {
    float x;
    float z;
    int owner_id;
    Game::Units::SpawnType spawn_type;
  };

  struct OwnerTroopTally {
    int owner_id{0};
    int troops{0};
  };

  static void process_barrack_capture(Engine::Core::World* world, float delta_time);
  static void tally_nearby_troops(std::span<const CaptureTroop> troops,
                                  float barrack_x,
                                  float barrack_z,
                                  float radius,
                                  std::vector<OwnerTroopTally>& out);
};

} // namespace Game::Systems
