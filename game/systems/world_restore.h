#pragma once

#include "../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Persistence {

struct WorldRestoreResult {

  Engine::Core::EntityID player_unit_id = Engine::Core::NULL_ENTITY;
};

[[nodiscard]] auto
rebuild_registries_after_load(Engine::Core::World* world,
                              int local_owner_id) -> WorldRestoreResult;

void rebuild_building_collisions(Engine::Core::World* world);

} // namespace Game::Persistence
