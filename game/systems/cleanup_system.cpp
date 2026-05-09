#include "cleanup_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "core/entity.h"
#include <algorithm>
#include <vector>

namespace Game::Systems {

void CleanupSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto casualty_entities = world->get_entities_with<
      Engine::Core::SoldierCasualtyAnimationComponent>();
  for (auto *entity : casualty_entities) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *casualties =
        entity
            ->get_component<Engine::Core::SoldierCasualtyAnimationComponent>();
    if (casualties == nullptr) {
      continue;
    }

    auto &entries = casualties->entries;
    for (auto &entry : entries) {
      entry.state_time += delta_time;
      if (entry.state == Engine::Core::DeathSequenceState::Dying &&
          entry.state_time >= entry.state_duration) {
        entry.state = Engine::Core::DeathSequenceState::DeadHold;
        entry.state_time = 0.0F;
      }
    }

    entries.erase(std::remove_if(
                      entries.begin(), entries.end(),
                      [](const auto &entry) {
                        return entry.state ==
                                   Engine::Core::DeathSequenceState::DeadHold &&
                               entry.state_time >= entry.dead_hold_duration;
                      }),
                  entries.end());
    if (entries.empty()) {
      entity
          ->remove_component<Engine::Core::SoldierCasualtyAnimationComponent>();
    }
  }

  auto dead_entities =
      world->get_entities_with<Engine::Core::DeathAnimationComponent>();
  for (auto *entity : dead_entities) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *death =
        entity->get_component<Engine::Core::DeathAnimationComponent>();
    auto *renderable =
        entity->get_component<Engine::Core::RenderableComponent>();
    if (death == nullptr) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
      continue;
    }

    death->state_time += delta_time;
    if (death->state == Engine::Core::DeathSequenceState::Dying &&
        death->state_time >= death->state_duration) {
      death->state = Engine::Core::DeathSequenceState::DeadHold;
      death->state_time = 0.0F;
    }

    if (death->state == Engine::Core::DeathSequenceState::DeadHold &&
        death->state_time >= death->dead_hold_duration) {
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
