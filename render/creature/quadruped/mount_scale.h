#pragma once

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Render::Creature::Quadruped {

[[nodiscard]] auto
mount_model_scale(const Engine::Core::World* world,
                  const Engine::Core::Entity* entity) noexcept -> float;

} // namespace Render::Creature::Quadruped
