#include "production_system_builder.h"

#include <algorithm>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/death_sequence.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "production_system_approach.h"
#include "production_system_builder_task.h"
#include "production_system_construction.h"
#include "production_system_food.h"
#include "production_system_repair.h"
#include "production_system_shared_site.h"
#include "production_system_wall.h"
#include "systems/builder_product_types.h"

namespace Game::Systems::ProductionTasks {

namespace {

constexpr float k_orphaned_task_limit_seconds = 8.0F;
constexpr float k_max_construction_distance_sq = 9.0F;
constexpr float k_work_spot_leeway = 1.0F;

struct BuilderTick {
  Engine::Core::World& world;
  Engine::Core::Entity& entity;
  Engine::Core::BuilderProductionComponent& builder;
  Engine::Core::TransformComponent* transform;
  Engine::Core::MovementComponent* movement;
  int owner_id;
  float delta_time;
  const FinishedSites& finishing_sites;
};

void tick_fault_display(Engine::Core::BuilderProductionComponent& builder,
                        float delta_time) {
  if (builder.fault_display_remaining > 0.0F) {
    builder.fault_display_remaining -= delta_time;
    if (builder.fault_display_remaining <= 0.0F) {
      builder.clear_fault();
    }
  }
}

void track_orphaned_task(BuilderTick& tick) {
  auto& builder = tick.builder;
  if (builder.has_task_target && !builder.has_construction_site) {
    builder.site_approach_seconds += tick.delta_time;
    if (builder.site_approach_seconds > k_orphaned_task_limit_seconds) {
      clear_builder_task_target(tick.world, &builder);
      reset_site_approach(builder);
      builder.report_fault(Engine::Core::BuilderTaskFault::TargetLost);
    }
  } else if (!builder.has_construction_site) {
    reset_site_approach(builder);
  }
}

auto wandered_off_site(const BuilderTick& tick) -> bool {
  if (!tick.builder.at_construction_site || tick.transform == nullptr) {
    return false;
  }
  if (is_gather_builder_product(tick.builder.product_type) &&
      tick.movement != nullptr) {
    float const dx = tick.builder.construction_site_x - tick.transform->position.x;
    float const dz = tick.builder.construction_site_z - tick.transform->position.z;
    float const reach = gather_bypass_reach(tick.movement->get_navigation_clearance()) +
                        k_work_spot_leeway;
    return (dx * dx) + (dz * dz) > reach * reach;
  }
  float const edge = distance_to_site_edge(
      tick.builder, tick.transform->position.x, tick.transform->position.z);
  return edge * edge > k_max_construction_distance_sq;
}

void interrupt_task(BuilderTick& tick) {
  auto& builder = tick.builder;
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.construction_complete = false;
  builder.time_remaining = 0.0F;
  clear_builder_task_target(tick.world, &builder);
  builder.report_fault(Engine::Core::BuilderTaskFault::Interrupted);
}

void tick_work_timer(const BuilderTick& tick) {
  auto& builder = tick.builder;
  if (raises_shared_site(builder)) {
    return;
  }
  builder.time_remaining -=
      is_gather_builder_product(builder.product_type)
          ? tick.delta_time * crew_gather_pace(tick.world, tick.entity.get_id())
          : tick.delta_time;
}

void hold_dismantle_or_finish(BuilderTick& tick) {
  auto& builder = tick.builder;
  auto const* dismantling =
      builder.structure_task_entity_id != 0
          ? tick.world.get_entity(builder.structure_task_entity_id)
          : nullptr;
  if (dismantling != nullptr && Engine::Core::is_live_entity(*dismantling)) {
    builder.time_remaining = builder.build_time;
    return;
  }

  builder.in_progress = false;
  builder.time_remaining = 0.0F;
  builder.construction_complete = true;
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  builder.structure_task_entity_id = 0;
  clear_builder_task_target(tick.world, &builder, false);
}

void run_repair_pass(BuilderTick& tick) {
  auto& builder = tick.builder;
  if (apply_structure_repair_tick(&tick.world, &builder)) {
    builder.time_remaining = builder.build_time;
    return;
  }
  finish_repair_task(tick.world, builder, tick.owner_id);
}

void finish_completed_task(BuilderTick& tick, bool raised_structure) {
  auto& builder = tick.builder;
  builder.in_progress = false;
  builder.time_remaining = 0.0F;
  builder.construction_complete = true;

  if (raised_structure) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent::for_owner(tick.owner_id,
                                               "build.construction_complete"));
  }
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  builder.construction_site_entity_id = 0;
  clear_builder_task_target(tick.world, &builder, false);
}

void complete_task(BuilderTick& tick) {
  auto& builder = tick.builder;
  bool raised_structure = false;

  auto* t = tick.world.try_get<Engine::Core::TransformComponent>(tick.entity.get_id());
  auto* u = tick.world.try_get<Engine::Core::UnitComponent>(tick.entity.get_id());
  if ((t != nullptr) && (u != nullptr)) {
    if (is_food_builder_product(builder.product_type)) {
      complete_food_task(tick.world, tick.entity, builder);
    } else if (is_harvest_builder_product(builder.product_type)) {
      complete_harvest_task(tick.world, tick.entity, builder);
    } else {
      auto const outcome = raise_structure(tick.world,
                                           tick.entity,
                                           builder,
                                           *t,
                                           *u,
                                           tick.movement,
                                           tick.finishing_sites);
      if (outcome == StructureOutcome::Skipped) {
        return;
      }
      raised_structure = outcome == StructureOutcome::Raised;
    }
  }
  finish_completed_task(tick, raised_structure);
}

void advance_working_builder(BuilderTick& tick) {
  auto& builder = tick.builder;
  if (wandered_off_site(tick)) {
    interrupt_task(tick);
    return;
  }

  settle_crew_at_posts(tick.world, tick.entity.get_id(), builder, tick.delta_time);
  if (!crew_at_posts(tick.world, tick.entity.get_id(), builder)) {
    return;
  }
  tick_work_timer(tick);
  if (builder.product_type == k_builder_product_repair &&
      builder.at_construction_site) {
    show_repair_in_progress(tick.world, builder);
  }
  publish_wall_progress(tick.world, builder);
  if (builder.product_type == k_builder_product_dismantle) {
    hold_dismantle_or_finish(tick);
    return;
  }
  if (builder.time_remaining > 0.0F) {
    return;
  }
  if (builder.product_type == k_builder_product_repair) {
    run_repair_pass(tick);
    return;
  }
  complete_task(tick);
}

void advance_builder(Engine::Core::World& world,
                     Engine::Core::Entity& entity,
                     Engine::Core::BuilderProductionComponent& builder,
                     float delta_time,
                     const FinishedSites& finishing_sites) {
  tick_fault_display(builder, delta_time);

  auto* transform = world.try_get<Engine::Core::TransformComponent>(entity.get_id());
  auto* movement = world.try_get<Engine::Core::MovementComponent>(entity.get_id());
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(entity.get_id());
  BuilderTick tick{world,
                   entity,
                   builder,
                   transform,
                   movement,
                   unit != nullptr ? unit->owner_id : 0,
                   delta_time,
                   finishing_sites};

  drop_lost_wall_site(world, builder);
  assign_queued_wall_site_if_idle(world, entity, builder);
  if (!refresh_food_task(world, entity, builder, transform, movement)) {
    return;
  }

  if (builder.has_construction_site && !builder.at_construction_site) {
    if (transform != nullptr) {
      advance_site_approach(world,
                            SiteApproachActor{.id = entity.get_id(),
                                              .owner_id = tick.owner_id,
                                              .transform = transform,
                                              .movement = movement},
                            builder,
                            delta_time);
    }
    return;
  }
  if (!builder.in_progress) {
    track_orphaned_task(tick);
    return;
  }
  advance_working_builder(tick);
}

} // namespace

void advance_builders(Engine::Core::World& world, float delta_time) {
  const auto finishing_sites = advance_shared_sites(world, delta_time);
  for (auto [entity_ref, builder_ref] :
       world.entity_view<Engine::Core::BuilderProductionComponent>()) {
    advance_builder(world, entity_ref, builder_ref, delta_time, finishing_sites);
  }
}

} // namespace Game::Systems::ProductionTasks
