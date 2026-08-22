#pragma once

namespace Engine::Core {
class Entity;
}

namespace Render::Creature::Quadruped {

[[nodiscard]] auto
mount_model_scale(const Engine::Core::Entity* entity) noexcept -> float;

} // namespace Render::Creature::Quadruped
