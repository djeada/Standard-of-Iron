#pragma once

#include "../core/component_presentation.h"
#include "../units/spawn_type.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

// Attaches the short-lived burst that marks a unit arriving in the world.
// Production uses it for a recruit stepping out of a building; the Iron
// Sepulcher uses it for every guardian a wave raises. The radius comes from the
// spawn type, so a skeleton and a barracks each get a burst that fits them.
void attach_spawn_flare(Engine::Core::World& world,
                        Engine::Core::EntityID entity_id,
                        Game::Units::SpawnType spawn_type,
                        Engine::Core::SpawnFlareStyle style);

} // namespace Game::Systems
