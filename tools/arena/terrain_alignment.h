#pragma once

namespace Engine::Core {
class Entity;
}

namespace Game::Map {
class TerrainService;
}

namespace Arena {

[[nodiscard]] auto
entity_keeps_planar_position(const Engine::Core::Entity& entity) -> bool;

void align_entity_to_ground(Engine::Core::Entity& entity,
                            const Game::Map::TerrainService& terrain);

} // namespace Arena
