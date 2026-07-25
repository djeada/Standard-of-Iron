#pragma once

namespace Engine::Core {
class Entity;
}

namespace Arena {

// Structures own the ground they stand on: their footprint is registered as an
// obstacle, so the walkable-cell snap that re-seats troops would push every
// building off its own plot each time the terrain is rebuilt. Buildings and
// construction sites therefore keep their planar position and only follow the
// new surface height.
[[nodiscard]] auto
entity_keeps_planar_position(const Engine::Core::Entity& entity) -> bool;

void align_entity_to_ground(Engine::Core::Entity& entity);

} // namespace Arena
