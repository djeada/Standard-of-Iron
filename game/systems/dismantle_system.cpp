#include "dismantle_system.h"

#include <algorithm>
#include <unordered_map>

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
  for (auto* worker :
       world->collect_entities_with<Engine::Core::BuilderProductionComponent>()) {
    auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder == nullptr || builder->structure_task_entity_id != structure_id ||
        builder->product_type != k_builder_product_dismantle) {
      continue;
    }
    builder->in_progress = false;
    builder->time_remaining = 0.0F;
    builder->construction_complete = true;
    builder->has_construction_site = false;
    builder->at_construction_site = false;
    builder->structure_task_entity_id = 0;
  }
}

} // namespace

void DismantleSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto sites = world->collect_entities_with<Engine::Core::DismantleSiteComponent>();
  if (sites.empty()) {
    return;
  }

  std::unordered_map<Engine::Core::EntityID, int> crew_by_structure;
  for (auto* worker :
       world->collect_entities_with<Engine::Core::BuilderProductionComponent>()) {
    const auto* builder =
        worker->get_component<Engine::Core::BuilderProductionComponent>();
    const auto* worker_unit = worker->get_component<Engine::Core::UnitComponent>();
    if (builder == nullptr || worker_unit == nullptr || worker_unit->health <= 0) {
      continue;
    }
    if (builder->structure_task_entity_id != 0 &&
        is_working_on(*builder, builder->structure_task_entity_id)) {
      crew_by_structure[builder->structure_task_entity_id] += 1;
    }
  }

  for (auto* structure : sites) {
    auto* site = structure->get_component<Engine::Core::DismantleSiteComponent>();
    auto* unit = structure->get_component<Engine::Core::UnitComponent>();
    if (site == nullptr || unit == nullptr || unit->health <= 0) {
      continue;
    }

    const auto crew_it = crew_by_structure.find(structure->get_id());
    const int crew = crew_it != crew_by_structure.end() ? crew_it->second : 0;
    site->active_workers = crew;

    if (crew == 0) {
      structure->remove_component<Engine::Core::DismantleSiteComponent>();
      continue;
    }

    const auto rate = static_cast<float>(std::min(crew, k_max_crew));
    site->progress += (rate * delta_time) / std::max(site->duration, 0.01F);
    if (site->progress < 1.0F) {
      continue;
    }

    grant_resources(
        unit->owner_id,
        dismantle_refund(Game::Units::spawn_typeToString(unit->spawn_type)));
    release_crew(world, structure->get_id());
    structure->remove_component<Engine::Core::DismantleSiteComponent>();
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent("build.construction_complete"));
    Combat::apply_unit_damage(world, structure, unit->max_health);
  }
}

} // namespace Game::Systems
