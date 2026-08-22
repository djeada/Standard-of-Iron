#include "dismantle_system.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "builder_product_types.h"
#include "combat_system/damage_application.h"
#include "construction_cost_catalog.h"
#include "player_resource_registry.h"
#include "resource_types.h"

namespace Game::Systems {

namespace {

auto is_working_on(const Engine::Core::BuilderProductionComponent& builder,
                   Engine::Core::EntityID structure_id) -> bool {
  return builder.structure_task_entity_id == structure_id &&
         builder.product_type == k_builder_product_dismantle && builder.in_progress;
}

void release_crew(Engine::Core::World* world, Engine::Core::EntityID structure_id) {
  for (auto [worker_id, builder] :
       world->view<Engine::Core::BuilderProductionComponent>()) {
    (void)worker_id;
    if (builder.structure_task_entity_id != structure_id ||
        builder.product_type != k_builder_product_dismantle) {
      continue;
    }
    auto* const builder_ptr = &builder;
    builder_ptr->in_progress = false;
    builder_ptr->time_remaining = 0.0F;
    builder_ptr->construction_complete = true;
    builder_ptr->has_construction_site = false;
    builder_ptr->at_construction_site = false;
    builder_ptr->structure_task_entity_id = 0;
  }
}

} // namespace

void DismantleSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  if (world->entities_with<Engine::Core::DismantleSiteComponent>().empty()) {
    return;
  }

  std::unordered_map<Engine::Core::EntityID, int> crew_by_structure;
  for (auto [worker_id, builder, worker_unit] :
       world->view<Engine::Core::BuilderProductionComponent,
                   Engine::Core::UnitComponent>()) {
    (void)worker_id;
    if (worker_unit.health <= 0) {
      continue;
    }
    if (builder.structure_task_entity_id != 0 &&
        is_working_on(builder, builder.structure_task_entity_id)) {
      crew_by_structure[builder.structure_task_entity_id] += 1;
    }
  }

  std::vector<Engine::Core::EntityID> finished;
  std::vector<Engine::Core::EntityID> abandoned;
  for (auto [structure_id, site, unit] :
       world->view<Engine::Core::DismantleSiteComponent,
                   Engine::Core::UnitComponent>()) {
    if (unit.health <= 0) {
      continue;
    }

    const auto crew_it = crew_by_structure.find(structure_id);
    const int crew = crew_it != crew_by_structure.end() ? crew_it->second : 0;
    site.active_workers = crew;

    if (crew == 0) {
      abandoned.push_back(structure_id);
      continue;
    }

    const auto rate = static_cast<float>(std::min(crew, k_max_crew));
    site.progress += (rate * delta_time) / std::max(site.duration, 0.01F);
    if (site.progress < 1.0F) {
      continue;
    }

    finished.push_back(structure_id);
  }

  for (const Engine::Core::EntityID structure_id : abandoned) {
    world->remove<Engine::Core::DismantleSiteComponent>(structure_id);
  }

  for (const Engine::Core::EntityID structure_id : finished) {
    auto* structure = world->get_entity(structure_id);
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(structure_id);
    if (structure == nullptr || unit == nullptr) {
      continue;
    }
    grant_resources(
        unit->owner_id,
        dismantle_refund(Game::Units::spawn_typeToString(unit->spawn_type)));
    release_crew(world, structure_id);
    world->remove<Engine::Core::DismantleSiteComponent>(structure_id);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent("build.construction_complete"));
    Combat::apply_unit_damage(world, structure, unit->max_health);
  }
}

} // namespace Game::Systems
