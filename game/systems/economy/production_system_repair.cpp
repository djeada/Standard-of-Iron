#include "production_system_repair.h"

#include <algorithm>

#include "core/component_gameplay.h"
#include "core/component_presentation.h"
#include "core/death_sequence.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "production_system_builder_task.h"
#include "systems/builder_product_types.h"
#include "systems/player_feedback.h"

namespace Game::Systems::ProductionTasks {

namespace {

constexpr float k_repair_fraction_per_tick = 0.06F;
constexpr int k_repair_minimum_per_tick = 8;

auto repair_target_of(Engine::Core::World* world,
                      const Engine::Core::BuilderProductionComponent* builder)
    -> Engine::Core::Entity* {
  if (world == nullptr || builder == nullptr ||
      builder->structure_task_entity_id == 0) {
    return nullptr;
  }
  auto* structure = world->get_entity(builder->structure_task_entity_id);
  return structure != nullptr && Engine::Core::is_live_entity(*structure) ? structure
                                                                          : nullptr;
}

auto structure_needs_repair(const Engine::Core::Entity* structure) -> bool {
  if (structure == nullptr) {
    return false;
  }
  const auto* unit = structure->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0 && unit->health < unit->max_health;
}

} // namespace

auto apply_structure_repair_tick(Engine::Core::World* world,
                                 Engine::Core::BuilderProductionComponent* builder)
    -> bool {
  auto* structure = repair_target_of(world, builder);
  if (!structure_needs_repair(structure)) {
    return false;
  }

  auto* unit = structure->get_component<Engine::Core::UnitComponent>();
  const int per_tick = std::max(k_repair_minimum_per_tick,
                                static_cast<int>(static_cast<float>(unit->max_health) *
                                                 k_repair_fraction_per_tick));
  restore_health(
      unit->owner_id, structure->get_id(), *unit, per_tick, unit->max_health);
  if (auto* shown = Engine::Core::get_or_add_component<
          Engine::Core::StructureRepairPresentationComponent>(*structure)) {
    shown->since_restore = 0.0F;
  }

  if (unit->health >= unit->max_health) {
    if (auto* fire = structure->get_component<Engine::Core::StructureFireComponent>()) {
      fire->remaining_duration = 0.0F;
      fire->ignition_progress = 0.0F;
      fire->tick_accumulator = 0.0F;
    }
    return false;
  }
  return true;
}

void show_repair_in_progress(Engine::Core::World& world,
                             const Engine::Core::BuilderProductionComponent& builder) {
  if (auto* structure = repair_target_of(&world, &builder);
      structure_needs_repair(structure)) {
    auto* shown = Engine::Core::get_or_add_component<
        Engine::Core::StructureRepairPresentationComponent>(*structure);
    if (shown != nullptr) {
      shown->active_for = std::max(shown->active_for, 0.5F);
    }
  }
}

void finish_repair_task(Engine::Core::World& world,
                        Engine::Core::BuilderProductionComponent& builder,
                        int owner_id) {
  if (repair_target_of(&world, &builder) == nullptr) {
    builder.report_fault(Engine::Core::BuilderTaskFault::TargetLost);
  } else {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent::for_owner(owner_id,
                                               "build.construction_complete"));
  }
  builder.in_progress = false;
  builder.time_remaining = 0.0F;
  builder.construction_complete = true;
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  builder.structure_task_entity_id = 0;
  clear_builder_task_target(world, &builder, false);
}

} // namespace Game::Systems::ProductionTasks
