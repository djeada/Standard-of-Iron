#include "mount_scale.h"

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/core/world.h"

namespace Render::Creature::Quadruped {

auto mount_model_scale(const Engine::Core::World* world,
                       const Engine::Core::Entity* entity) noexcept -> float {
  if (world == nullptr || entity == nullptr) {
    return 1.0F;
  }
  const auto* transform =
      world->try_get<Engine::Core::TransformComponent>(entity->get_id());
  if (transform == nullptr) {
    return 1.0F;
  }
  return transform->scale.x > 0.01F ? transform->scale.x : 1.0F;
}

} // namespace Render::Creature::Quadruped
