#include "cleanup_system.h"

#include <algorithm>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "core/entity.h"

namespace Game::Systems {

namespace {

using Engine::Core::EntityID;
using Engine::Core::World;

template <typename T>
void drop_components(World& world, const std::vector<EntityID>& entity_ids) {
  for (const EntityID entity_id : entity_ids) {
    world.remove<T>(entity_id);
  }
}

} // namespace

void CleanupSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto [entity_id, blood_stain] :
       world->view<Engine::Core::BloodStainComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }
    blood_stain.elapsed_time += delta_time;
    if (blood_stain.elapsed_time >= blood_stain.lifetime) {
      world->emplace<Engine::Core::PendingRemovalComponent>(entity_id);
    }
  }

  std::vector<EntityID> expired;
  for (auto [entity_id, presentation] :
       world->view<Engine::Core::StructureDamagePresentationComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }
    for (auto& impact : presentation.impacts) {
      impact.age += std::max(0.0F, delta_time);
    }
    std::erase_if(presentation.impacts,
                  [](auto const& impact) { return impact.age >= impact.lifetime; });
    if (presentation.impacts.empty()) {
      expired.push_back(entity_id);
    }
  }
  drop_components<Engine::Core::StructureDamagePresentationComponent>(*world, expired);
  expired.clear();

  for (auto [entity_id, presentation] :
       world->view<Engine::Core::RpgContactPresentationComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }
    for (auto& contact : presentation.entries) {
      contact.age += std::max(0.0F, delta_time);
    }
    std::erase_if(presentation.entries,
                  [](auto const& contact) { return contact.age >= contact.lifetime; });
    if (presentation.entries.empty()) {
      expired.push_back(entity_id);
    }
  }
  drop_components<Engine::Core::RpgContactPresentationComponent>(*world, expired);
  expired.clear();

  for (auto [entity_id, casualties] :
       world->view<Engine::Core::SoldierCasualtyAnimationComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    auto& entries = casualties.entries;
    for (auto& entry : entries) {
      entry.state_time += delta_time;
      if (entry.state == Engine::Core::DeathSequenceState::Dying &&
          entry.state_time >= entry.state_duration) {
        entry.state = Engine::Core::DeathSequenceState::DeadHold;
        entry.state_time = 0.0F;
      }
    }

    std::erase_if(entries, [](const auto& entry) {
      return entry.state == Engine::Core::DeathSequenceState::DeadHold &&
             entry.state_time >= entry.dead_hold_duration;
    });
    if (entries.empty()) {
      expired.push_back(entity_id);
    }
  }
  drop_components<Engine::Core::SoldierCasualtyAnimationComponent>(*world, expired);

  for (auto [entity_id, death] : world->view<Engine::Core::DeathAnimationComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    death.state_time += delta_time;
    if (death.state == Engine::Core::DeathSequenceState::Dying &&
        death.state_time >= death.state_duration) {
      death.state = Engine::Core::DeathSequenceState::DeadHold;
      death.state_time = 0.0F;
    }

    if (death.state == Engine::Core::DeathSequenceState::DeadHold &&
        death.state_time >= death.dead_hold_duration) {
      if (auto* renderable =
              world->try_get<Engine::Core::RenderableComponent>(entity_id)) {
        renderable->visible = false;
      }
      world->emplace<Engine::Core::PendingRemovalComponent>(entity_id);
    }
  }

  remove_dead_entities(world);
}

void CleanupSystem::remove_dead_entities(Engine::Core::World* world) {
  std::vector<Engine::Core::EntityID> entities_to_remove;

  const auto pending = world->entities_with<Engine::Core::PendingRemovalComponent>();
  entities_to_remove.assign(pending.begin(), pending.end());

  for (auto entity_id : entities_to_remove) {
    world->destroy_entity(entity_id);
  }
}

} // namespace Game::Systems
