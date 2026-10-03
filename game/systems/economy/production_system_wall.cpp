#include "production_system_wall.h"

#include <algorithm>

#include "build_site.h"
#include "construction_cost_catalog.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "production_system_builder_task.h"
#include "systems/builder_product_types.h"
#include "systems/navigation/walkability.h"
#include "systems/navigation/wall_network_service.h"
#include "systems/owner_queries.h"
#include "systems/player_feedback.h"
#include "systems/player_resource_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems::ProductionTasks {

namespace {

auto assign_next_wall_site(Engine::Core::World* world,
                           Engine::Core::Entity* builder_entity,
                           Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (world == nullptr || builder_entity == nullptr || builder == nullptr) {
    return false;
  }

  while (!builder->queued_construction_site_ids.empty()) {
    const auto site_id = builder->queued_construction_site_ids.front();
    builder->queued_construction_site_ids.erase(
        builder->queued_construction_site_ids.begin());
    auto* site_entity = world->get_entity(site_id);
    auto* site_transform =
        site_entity != nullptr
            ? world->try_get<Engine::Core::TransformComponent>(site_entity->get_id())
            : nullptr;
    auto* site = site_entity != nullptr
                     ? world->try_get<Engine::Core::WallConstructionSiteComponent>(
                           site_entity->get_id())
                     : nullptr;
    if (site_transform == nullptr || site == nullptr) {
      continue;
    }

    builder->construction_site_entity_id = site_id;
    builder->has_construction_site = true;
    builder->construction_site_x = site_transform->position.x;
    builder->construction_site_z = site_transform->position.z;
    builder->at_construction_site = false;
    builder->in_progress = false;
    builder->build_time = site->build_time;
    builder->time_remaining = site->build_time;
    builder->construction_complete = false;
    builder->bypass_movement_active = false;

    if (auto* movement =
            world->try_get<Engine::Core::MovementComponent>(builder_entity->get_id())) {
      movement->set_rest_position(builder->construction_site_x,
                                  builder->construction_site_z);
    }
    return true;
  }

  builder->construction_site_entity_id = 0;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  return false;
}

} // namespace

auto skip_invalid_wall_site(Engine::Core::World* world,
                            Engine::Core::Entity* builder_entity,
                            Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (world == nullptr || builder_entity == nullptr || builder == nullptr ||
      builder->construction_site_entity_id == 0) {
    return false;
  }

  auto* site_entity = world->get_entity(builder->construction_site_entity_id);
  auto* site = site_entity != nullptr
                   ? world->try_get<Engine::Core::WallConstructionSiteComponent>(
                         site_entity->get_id())
                   : nullptr;
  auto* transform =
      site_entity != nullptr
          ? world->try_get<Engine::Core::TransformComponent>(site_entity->get_id())
          : nullptr;
  auto* wall =
      site_entity != nullptr
          ? world->try_get<Engine::Core::WallSegmentComponent>(site_entity->get_id())
          : nullptr;
  if (site_entity == nullptr || site == nullptr || transform == nullptr) {
    return false;
  }

  if (site->product_type == Game::Units::SpawnType::WallLadder) {
    // A ladder stays valid while the wall it leans on still stands.
    if (Game::Systems::WallNetworkService::find_ladder_placement(*world,
                                                                 site->owner_id,
                                                                 transform->position.x,
                                                                 transform->position.z,
                                                                 site_entity->get_id())
            .valid) {
      return false;
    }
  } else {
    Game::Systems::WallGridPosition const position =
        wall != nullptr ? Game::Systems::WallGridPosition{wall->grid_x, wall->grid_z}
                        : Game::Systems::WallNetworkService::snap_world_position(
                              transform->position.x, transform->position.z);
    const auto validation =
        Game::Systems::WallNetworkService::validate_wall_segment_placement(
            *world,
            position,
            Game::Systems::wall_ground_probe(*world),
            true,
            site_entity->get_id());
    if (validation.valid) {
      return false;
    }
  }

  const auto refund =
      construction_cost_info(Game::Units::spawn_typeToString(site->product_type))
          .resource_costs;
  grant_resources_at(site->owner_id,
                     transform->position.x,
                     transform->position.y,
                     transform->position.z,
                     refund);
  world->destroy_entity(site_entity->get_id());
  builder->construction_site_entity_id = 0;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = false;
  builder->bypass_movement_active = false;
  clear_builder_task_target(*world, builder, false);
  builder->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
  WallNetworkService::refresh_world(*world);
  assign_next_wall_site(world, builder_entity, builder);
  return true;
}

void drop_lost_wall_site(Engine::Core::World& world,
                         Engine::Core::BuilderProductionComponent& builder) {
  if (!is_wall_network_product(builder.product_type) ||
      builder.construction_site_entity_id == 0 ||
      world.get_entity(builder.construction_site_entity_id) != nullptr) {
    return;
  }
  builder.construction_site_entity_id = 0;
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.time_remaining = 0.0F;
  builder.report_fault(Engine::Core::BuilderTaskFault::TargetLost);
}

void assign_queued_wall_site_if_idle(
    Engine::Core::World& world,
    Engine::Core::Entity& builder_entity,
    Engine::Core::BuilderProductionComponent& builder) {
  if (is_wall_network_product(builder.product_type) && !builder.has_construction_site &&
      !builder.in_progress && !builder.queued_construction_site_ids.empty()) {
    assign_next_wall_site(&world, &builder_entity, &builder);
  }
}

void publish_wall_progress(Engine::Core::World& world,
                           const Engine::Core::BuilderProductionComponent& builder) {
  if (!is_wall_network_product(builder.product_type) ||
      builder.construction_site_entity_id == 0) {
    return;
  }
  auto* site_entity = world.get_entity(builder.construction_site_entity_id);
  if (site_entity == nullptr) {
    return;
  }
  if (auto* site =
          site_entity->get_component<Engine::Core::WallConstructionSiteComponent>()) {
    const float duration = std::max(builder.build_time, 0.001F);
    site->progress = std::clamp(1.0F - builder.time_remaining / duration, 0.0F, 1.0F);
  }
}

} // namespace Game::Systems::ProductionTasks
