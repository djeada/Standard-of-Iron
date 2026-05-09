#include "cleanup_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "core/entity.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Game::Systems {

void CleanupSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto dead_entities = world->get_entities_with<Engine::Core::DeathMotionComponent>();
  for (auto *entity : dead_entities) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *death = entity->get_component<Engine::Core::DeathMotionComponent>();
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    auto *renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (death == nullptr || transform == nullptr) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
      continue;
    }

    death->elapsed_time += delta_time;
    float const progress =
        (death->duration > 0.001F)
            ? std::clamp(death->elapsed_time / death->duration, 0.0F, 1.0F)
            : 1.0F;
    float const impulse_falloff = 1.0F - progress;
    transform->position.x += death->impulse_x * impulse_falloff * delta_time;
    transform->position.z += death->impulse_z * impulse_falloff * delta_time;

    float pitch = 65.0F * progress;
    if (death->reaction == Engine::Core::DeathReactionType::Crushed) {
      pitch = 18.0F * progress;
      transform->position.y -= 0.18F * progress * delta_time;
    } else if (death->reaction == Engine::Core::DeathReactionType::Thrown) {
      float const arc = std::sin(progress * std::numbers::pi_v<float>) * 0.20F;
      transform->position.y += arc * delta_time;
      pitch = 88.0F * progress;
    } else if (death->reaction == Engine::Core::DeathReactionType::SpinFall) {
      pitch = 72.0F * progress;
    } else if (death->reaction == Engine::Core::DeathReactionType::BackwardFall) {
      pitch = 78.0F * progress;
    }

    transform->rotation.x = pitch;
    transform->rotation.z += death->angular_velocity * impulse_falloff *
                             delta_time;

    if (progress >= 1.0F) {
      if (renderable != nullptr) {
        renderable->visible = false;
      }
      entity->add_component<Engine::Core::PendingRemovalComponent>();
    }
  }

  remove_dead_entities(world);
}

void CleanupSystem::remove_dead_entities(Engine::Core::World *world) {
  std::vector<Engine::Core::EntityID> entities_to_remove;

  auto entities =
      world->get_entities_with<Engine::Core::PendingRemovalComponent>();

  entities_to_remove.reserve(entities.size());
  for (auto *entity : entities) {
    entities_to_remove.push_back(entity->get_id());
  }

  for (auto entity_id : entities_to_remove) {
    world->destroy_entity(entity_id);
  }
}

} // namespace Game::Systems
