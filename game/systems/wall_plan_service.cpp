#include "wall_plan_service.h"

#include <cmath>

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../units/spawn_type.h"
#include "construction_cost_catalog.h"
#include "economy_feedback.h"
#include "nation_registry.h"
#include "nav_grid.h"
#include "order_service.h"
#include "player_resource_registry.h"
#include "resource_types.h"

namespace Game::Systems {

namespace {

auto normalize_degrees(float angle) -> float {
  while (angle < 0.0F) {
    angle += 360.0F;
  }
  while (angle >= 360.0F) {
    angle -= 360.0F;
  }
  return angle;
}

auto is_vertical(float rotation_y) -> bool {
  const int quarter_turns =
      static_cast<int>(std::round(normalize_degrees(rotation_y) / 90.0F));
  return (quarter_turns % 2) != 0;
}

auto item_type(const WallPlanRequest& request) -> const char* {
  return request.gate ? "wall_gate" : "wall_segment";
}

auto nation_of(const Engine::Core::World& world, int owner_id) -> NationID {
  auto& nations = *Game::Session::services_for(world).nations;
  if (const auto* nation = nations.get_nation_for_player(owner_id)) {
    return nation->id;
  }
  return nations.default_nation_id();
}

} // namespace

auto WallPlanService::plan(Engine::Core::World& world,
                           const WallPlanRequest& request) -> WallPlan {
  WallPlan plan;
  plan.wood_per_segment =
      construction_cost_info(item_type(request)).resource_costs.get(ResourceType::Wood);

  const auto chain = request.gate ? std::vector<WallGridPosition>{request.target}
                                  : WallNetworkService::build_axis_aligned_chain(
                                        request.anchor, request.target);

  WallNetworkService::OccupancySet occupancy;
  WallNetworkService::add_world_occupancy(world, occupancy, true);
  WallNetworkService::OwnerOccupancyMap connection_occupancy_by_owner;
  WallNetworkService::build_connection_occupancy(
      world, connection_occupancy_by_owner, true, true);

  int available_wood = Game::Session::services_for(world).economy->get(
      request.owner_id, ResourceType::Wood);

  for (const auto& grid_pos : chain) {
    const auto key = WallNetworkService::encode_key(grid_pos.x, grid_pos.z);
    PlannedWallSegment segment;
    segment.grid_x = grid_pos.x;
    segment.grid_z = grid_pos.z;
    segment.world_position = NavGrid::grid_to_world(Point{grid_pos.x, grid_pos.z});

    if (occupancy.find(key) != occupancy.end()) {
      segment.fault = WallSegmentFault::Occupied;
    } else if (const auto validation =
                   WallNetworkService::validate_wall_segment_placement(
                       world, grid_pos, true);
               !validation.valid) {
      segment.fault = WallSegmentFault::Invalid;
      segment.failure_reason = validation.failure_reason;
    } else if (available_wood < plan.wood_per_segment) {
      segment.fault = WallSegmentFault::NotEnoughWood;
    } else {
      segment.valid = true;
      ++plan.valid_count;
      available_wood -= plan.wood_per_segment;
      occupancy.insert(key);
    }
    plan.segments.push_back(segment);
  }

  const auto nation_id = nation_of(world, request.owner_id);
  auto preview_occupancy = connection_occupancy_by_owner.contains(request.owner_id)
                               ? connection_occupancy_by_owner.at(request.owner_id)
                               : WallNetworkService::OccupancySet{};
  for (const auto& segment : plan.segments) {
    if (segment.valid) {
      preview_occupancy.insert(
          WallNetworkService::encode_key(segment.grid_x, segment.grid_z));
    }
  }

  auto& terrain = *Game::Session::services_for(world).terrain;
  for (auto& segment : plan.segments) {
    segment.connection_mask = WallNetworkService::compute_connection_mask(
        preview_occupancy, segment.grid_x, segment.grid_z);
    const auto appearance =
        request.gate ? WallNetworkService::resolve_gate_appearance(
                           nation_id, segment.connection_mask, request.rotation_y)
                     : WallNetworkService::resolve_appearance(nation_id,
                                                              segment.connection_mask);
    segment.rotation_y = appearance.rotation_y;
    if (chain.size() == 1U && segment.connection_mask == 0U) {
      segment.rotation_y = is_vertical(request.rotation_y) ? 90.0F : 0.0F;
    }
    if (terrain.is_initialized()) {
      segment.world_position = terrain.resolve_surface_world_position(
          segment.world_position.x(), segment.world_position.z(), 0.0F, 0.0F);
    }
  }
  return plan;
}

auto WallPlanService::commit(Engine::Core::World& world,
                             const WallPlanRequest& request,
                             const WallPlan& plan,
                             const std::vector<Engine::Core::EntityID>& builders)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> site_ids;
  if (plan.valid_count <= 0 || builders.empty()) {
    return site_ids;
  }

  ResourceAmounts total_cost;
  total_cost.set(ResourceType::Wood, plan.wood_cost());
  auto& resources = *Game::Session::services_for(world).economy;
  if (!resources.has_at_least(request.owner_id, total_cost)) {
    return site_ids;
  }
  resources.spend(request.owner_id, total_cost);

  QVector3D plan_centroid;
  for (const auto& segment : plan.segments) {
    if (segment.valid) {
      plan_centroid += segment.world_position;
    }
  }
  plan_centroid /= static_cast<float>(plan.valid_count);
  publish_resource_bundle_at(request.owner_id,
                             plan_centroid.x(),
                             plan_centroid.y(),
                             plan_centroid.z(),
                             total_cost,
                             -1);

  const auto nation_id = nation_of(world, request.owner_id);
  const std::string product = item_type(request);
  const float build_time = construction_build_time(product);

  site_ids.reserve(plan.valid_count);
  for (const auto& segment : plan.segments) {
    if (!segment.valid) {
      continue;
    }
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      continue;
    }
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* renderable = entity->add_component<Engine::Core::RenderableComponent>();
    auto* wall = entity->add_component<Engine::Core::WallSegmentComponent>();
    auto* site = entity->add_component<Engine::Core::WallConstructionSiteComponent>();
    if (transform == nullptr || renderable == nullptr || wall == nullptr ||
        site == nullptr) {
      world.destroy_entity(entity->get_id());
      continue;
    }

    transform->position = {segment.world_position.x(),
                           segment.world_position.y(),
                           segment.world_position.z()};
    transform->rotation = {0.0F, segment.rotation_y, 0.0F};
    transform->scale = {1.0F, 1.0F, 1.0F};

    renderable->visible = false;
    renderable->renderer_id =
        (request.gate ? WallNetworkService::resolve_gate_appearance(
                            nation_id, segment.connection_mask, segment.rotation_y)
                      : WallNetworkService::resolve_appearance(nation_id,
                                                               segment.connection_mask))
            .renderer_id;

    wall->grid_x = segment.grid_x;
    wall->grid_z = segment.grid_z;
    wall->connection_mask = segment.connection_mask;

    site->owner_id = request.owner_id;
    site->nation_id = nation_id;
    site->build_time = build_time;
    site->progress = 0.0F;
    site->product_type = request.gate ? Game::Units::SpawnType::WallGate
                                      : Game::Units::SpawnType::WallSegment;

    site_ids.push_back(entity->get_id());
  }

  WallNetworkService::refresh_world(world);

  std::vector<std::vector<Engine::Core::EntityID>> assignments(builders.size());
  for (std::size_t i = 0; i < site_ids.size(); ++i) {
    assignments[i % assignments.size()].push_back(site_ids[i]);
  }

  for (std::size_t index = 0; index < builders.size(); ++index) {
    auto* entity = world.get_entity(builders[index]);
    auto* builder =
        entity != nullptr
            ? entity->get_component<Engine::Core::BuilderProductionComponent>()
            : nullptr;
    if (builder == nullptr) {
      continue;
    }
    OrderService::clear_builder_task(world, entity);
    OrderService::clear_builder_gather_order(entity);
    builder->clear_auto_gather();
    if (assignments[index].empty()) {
      continue;
    }

    builder->product_type = product;
    builder->build_time = build_time;
    builder->time_remaining = build_time;
    builder->construction_complete = false;
    builder->queued_construction_site_ids = assignments[index];
    builder->construction_site_entity_id =
        builder->queued_construction_site_ids.front();
    builder->queued_construction_site_ids.erase(
        builder->queued_construction_site_ids.begin());

    auto* site_entity = world.get_entity(builder->construction_site_entity_id);
    auto* site_transform =
        site_entity != nullptr
            ? site_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (site_transform == nullptr) {
      OrderService::clear_builder_task(world, entity);
      continue;
    }
    builder->has_construction_site = true;
    builder->construction_site_x = site_transform->position.x;
    builder->construction_site_z = site_transform->position.z;
    builder->construction_site_rotation_y = site_transform->rotation.y;
    if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
      movement->set_rest_position(builder->construction_site_x,
                                  builder->construction_site_z);
    }
  }
  return site_ids;
}

} // namespace Game::Systems
