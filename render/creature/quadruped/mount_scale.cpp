#include "mount_scale.h"

#include "game/core/component.h"
#include "game/core/entity.h"

namespace Render::Creature::Quadruped {

auto mount_model_scale(const Engine::Core::Entity* entity) noexcept -> float {
  if (entity == nullptr) {
    return 1.0F;
  }
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return 1.0F;
  }
  return transform->scale.x > 0.01F ? transform->scale.x : 1.0F;
}

} // namespace Render::Creature::Quadruped
