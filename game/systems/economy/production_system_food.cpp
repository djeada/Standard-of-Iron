#include "production_system_food.h"

#include <QVector3D>

#include <algorithm>
#include <optional>

#include "core/component_economy.h"
#include "core/component_gameplay.h"
#include "core/death_sequence.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "food_targets.h"
#include "production_system_builder_task.h"
#include "systems/builder_product_types.h"
#include "systems/harvest_yields.h"
#include "systems/player_feedback.h"
#include "systems/spawn_flare.h"

namespace Game::Systems::ProductionTasks {

namespace {

auto food_target_position(Engine::Core::World* world,
                          const Engine::Core::BuilderProductionComponent* builder)
    -> std::optional<QVector3D> {
  if (world == nullptr || builder == nullptr ||
      builder->structure_task_entity_id == 0) {
    return std::nullopt;
  }
  auto* target = world->get_entity(builder->structure_task_entity_id);
  const auto* transform =
      target != nullptr
          ? world->try_get<Engine::Core::TransformComponent>(target->get_id())
          : nullptr;
  if (transform == nullptr) {
    return std::nullopt;
  }
  return QVector3D(transform->position.x, 0.0F, transform->position.z);
}

auto food_target_still_valid(Engine::Core::World* world,
                             const Engine::Core::Entity* worker,
                             const Engine::Core::BuilderProductionComponent* builder)
    -> bool {
  if (world == nullptr || worker == nullptr || builder == nullptr) {
    return false;
  }
  auto* target = world->get_entity(builder->structure_task_entity_id);
  if (target == nullptr) {
    return false;
  }
  if (builder->product_type == k_builder_product_harvest_grain) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(worker->get_id());
    return unit != nullptr && farm_is_harvestable(*target, unit->owner_id);
  }
  return sheep_is_slaughterable(*target);
}

void abandon_food_task(Engine::Core::World& world,
                       Engine::Core::BuilderProductionComponent* builder,
                       Engine::Core::BuilderTaskFault fault) {
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = false;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->bypass_movement_active = false;
  builder->structure_task_entity_id = 0;
  clear_builder_task_target(world, builder, false);
  builder->report_fault(fault);
}

void hold_sheep_still(Engine::Core::World* world,
                      const Engine::Core::BuilderProductionComponent* builder) {
  auto* sheep = world->get_entity(builder->structure_task_entity_id);
  if (sheep == nullptr) {
    return;
  }
  if (auto* wildlife =
          world->try_get<Engine::Core::WildlifeComponent>(sheep->get_id())) {
    wildlife->held_timer = std::max(wildlife->held_timer, 0.75F);
    if (builder->in_progress) {
      wildlife->dazed_timer = std::max(
          wildlife->dazed_timer, Engine::Core::WildlifeComponent::k_dazed_hold_seconds);
    }
  }
  if (auto* movement =
          world->try_get<Engine::Core::MovementComponent>(sheep->get_id())) {
    if (movement->get_has_target()) {
      movement->stop();
    }
  }
}

void slaughter_sheep(Engine::Core::World* world,
                     Engine::Core::Entity* worker,
                     Engine::Core::EntityID sheep_id) {
  auto* sheep = world->get_entity(sheep_id);
  auto* unit = sheep != nullptr
                   ? world->try_get<Engine::Core::UnitComponent>(sheep->get_id())
                   : nullptr;
  if (unit == nullptr) {
    return;
  }
  unit->health = 0;
  if (auto* movement =
          world->try_get<Engine::Core::MovementComponent>(sheep->get_id())) {
    movement->stop();
  }
  Engine::Core::begin_death_sequence(*sheep, 0U);
  const auto* worker_unit =
      worker != nullptr ? world->try_get<Engine::Core::UnitComponent>(worker->get_id())
                        : nullptr;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(sheep_id,
                                  unit->owner_id,
                                  unit->spawn_type,
                                  worker != nullptr ? worker->get_id() : 0,
                                  worker_unit != nullptr ? worker_unit->owner_id : 0));
}

auto complete_food_harvest(Engine::Core::World* world,
                           Engine::Core::Entity* worker,
                           Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (!food_target_still_valid(world, worker, builder)) {
    return false;
  }
  auto* target = world->get_entity(builder->structure_task_entity_id);
  int reward = 0;
  auto form = Engine::Core::CarriedFoodForm::Grain;
  if (builder->product_type == k_builder_product_harvest_grain) {
    auto* crop = world->try_get<Engine::Core::FarmComponent>(target->get_id());
    if (crop == nullptr) {
      return false;
    }
    crop->reset_after_harvest();
    attach_harvest_flare(*world, target->get_id());
    reward = k_harvest_grain_food_reward;
  } else {
    slaughter_sheep(world, worker, target->get_id());
    reward = k_slaughter_sheep_food_reward;

    form = Engine::Core::CarriedFoodForm::Meat;
  }
  load_onto_hauler(worker, ResourceType::Food, reward, form);
  return true;
}

void chase_sheep(Engine::Core::World& world,
                 Engine::Core::Entity& worker,
                 Engine::Core::BuilderProductionComponent& builder,
                 const Engine::Core::TransformComponent& transform,
                 Engine::Core::MovementComponent* movement) {
  auto const sheep_position = food_target_position(&world, &builder);
  if (!sheep_position.has_value()) {
    return;
  }
  float const dx = sheep_position->x() - transform.position.x;
  float const dz = sheep_position->z() - transform.position.z;
  float const dist_sq = dx * dx + dz * dz;
  if (dist_sq <= k_sheep_work_reach * k_sheep_work_reach) {
    hold_sheep_still(&world, &builder);
    if (!builder.at_construction_site) {
      builder.construction_site_x = sheep_position->x();
      builder.construction_site_z = sheep_position->z();
      builder.task_target_x = sheep_position->x();
      builder.task_target_z = sheep_position->z();
    }
    return;
  }

  QVector3D const work_position =
      food_work_position(world,
                         worker.get_id(),
                         QVector3D(transform.position.x, 0.0F, transform.position.z),
                         FoodTarget{.id = builder.structure_task_entity_id,
                                    .product_type = k_builder_product_slaughter_sheep,
                                    .x = sheep_position->x(),
                                    .z = sheep_position->z()});
  builder.construction_site_x = work_position.x();
  builder.construction_site_z = work_position.z();
  builder.task_target_x = sheep_position->x();
  builder.task_target_z = sheep_position->z();
  if (builder.at_construction_site) {
    builder.at_construction_site = false;
    builder.in_progress = false;
    builder.has_construction_site = true;
  }
  builder.bypass_movement_active = false;
  if (movement != nullptr) {
    movement->set_rest_position(work_position.x(), work_position.z());
  }
}

} // namespace

auto refresh_food_task(Engine::Core::World& world,
                       Engine::Core::Entity& worker,
                       Engine::Core::BuilderProductionComponent& builder,
                       const Engine::Core::TransformComponent* transform,
                       Engine::Core::MovementComponent* movement) -> bool {
  if (!is_food_builder_product(builder.product_type) ||
      builder.structure_task_entity_id == 0 ||
      !(builder.has_construction_site || builder.in_progress)) {
    return true;
  }
  if (!food_target_still_valid(&world, &worker, &builder)) {
    abandon_food_task(world, &builder, Engine::Core::BuilderTaskFault::TargetLost);
    return false;
  }
  if (builder.product_type == k_builder_product_slaughter_sheep &&
      transform != nullptr) {
    chase_sheep(world, worker, builder, *transform, movement);
  }
  return true;
}

void complete_food_task(Engine::Core::World& world,
                        Engine::Core::Entity& worker,
                        Engine::Core::BuilderProductionComponent& builder) {
  float const anchor_x = builder.task_target_x;
  float const anchor_z = builder.task_target_z;
  if (complete_food_harvest(&world, &worker, &builder)) {
    if (!world.has<Engine::Core::AIControlledComponent>(worker.get_id())) {
      builder.has_gather_order = true;
      builder.gather_product_type = builder.product_type;
      builder.gather_anchor_x = anchor_x;
      builder.gather_anchor_z = anchor_z;
    }
  } else {
    builder.report_fault(Engine::Core::BuilderTaskFault::TargetLost);
  }
  builder.structure_task_entity_id = 0;
}

} // namespace Game::Systems::ProductionTasks
