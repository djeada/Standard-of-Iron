#pragma once

#include "../core/component_presentation.h"
#include "../units/spawn_type.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

void attach_spawn_flare(Engine::Core::World& world,
                        Engine::Core::EntityID entity_id,
                        Game::Units::SpawnType spawn_type,
                        Engine::Core::SpawnFlareStyle style);

void attach_harvest_flare(Engine::Core::World& world, Engine::Core::EntityID field_id);

} // namespace Game::Systems
