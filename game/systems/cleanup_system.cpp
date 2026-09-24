#include "cleanup_system.h"

#include <algorithm>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/death_sequence.h"
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

struct SettledCorpse {
  Engine::Core::DeathSequenceState* state{nullptr};
  float* state_time{nullptr};
  float age{0.0F};
};

template <typename Sequence>
void note_settled_corpse(Sequence& sequence, std::vector<SettledCorpse>& settled) {
  if (Engine::Core::death_sequence_is_settled(sequence)) {
    settled.push_back({&sequence.state, &sequence.state_time, sequence.state_time});
  }
}

void enforce_corpse_budget(std::vector<SettledCorpse>& settled) {
  auto const budget = static_cast<std::size_t>(Engine::Core::Defaults::k_corpse_budget);
  if (settled.size() <= budget) {
    return;
  }
  std::sort(settled.begin(), settled.end(), [](auto const& lhs, auto const& rhs) {
    return lhs.age > rhs.age;
  });
  for (std::size_t index = 0; index < settled.size() - budget; ++index) {
    *settled[index].state = Engine::Core::DeathSequenceState::Sinking;
    *settled[index].state_time = 0.0F;
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

  using RepairShown = Engine::Core::StructureRepairPresentationComponent;
  for (auto [entity_id, repair] : world->view<RepairShown>()) {
    float const dt = std::max(0.0F, delta_time);
    repair.active_for = std::max(0.0F, repair.active_for - dt);
    repair.since_restore =
        std::min(repair.since_restore + dt, RepairShown::k_idle_since_restore);
    float const step = dt / RepairShown::k_scaffold_seconds;
    repair.scaffold = std::clamp(
        repair.scaffold + (repair.active_for > 0.0F ? step : -step), 0.0F, 1.0F);
    if (repair.active_for <= 0.0F && repair.scaffold <= 0.0F) {
      expired.push_back(entity_id);
    }
  }
  drop_components<RepairShown>(*world, expired);
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

  std::vector<SettledCorpse> settled;
  for (auto [entity_id, casualties] :
       world->view<Engine::Core::SoldierCasualtyAnimationComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    auto& entries = casualties.entries;
    std::erase_if(entries, [delta_time](auto& entry) {
      return Engine::Core::advance_death_sequence(entry, delta_time);
    });
    for (auto& entry : entries) {
      note_settled_corpse(entry, settled);
    }
    if (entries.empty()) {
      expired.push_back(entity_id);
    }
  }
  drop_components<Engine::Core::SoldierCasualtyAnimationComponent>(*world, expired);

  for (auto [entity_id, death] : world->view<Engine::Core::DeathAnimationComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    if (Engine::Core::advance_death_sequence(death, delta_time)) {
      if (auto* renderable =
              world->try_get<Engine::Core::RenderableComponent>(entity_id)) {
        renderable->visible = false;
      }
      world->emplace<Engine::Core::PendingRemovalComponent>(entity_id);
      continue;
    }
    // Ruins keep their own timing; the budget exists to cap soldier corpses.
    if (death.profile != Engine::Core::DeathSequenceProfile::Structure) {
      note_settled_corpse(death, settled);
    }
  }
  enforce_corpse_budget(settled);

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
