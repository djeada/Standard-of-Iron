#pragma once

#include "game/core/entity.h"

namespace Engine::Core {
class World;
}

namespace Arena {

[[nodiscard]] auto
entity_keeps_planar_position(Engine::Core::World& world,
                             Engine::Core::EntityID entity_id) -> bool;

void align_entity_to_ground(Engine::Core::World& world,
                            Engine::Core::EntityID entity_id);

} // namespace Arena
