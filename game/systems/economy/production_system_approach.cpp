#include "production_system_approach.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "build_site.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "production_system_builder_task.h"
#include "systems/builder_product_types.h"
#include "systems/building_collision_registry.h"
#include "systems/movement/command_service.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"

namespace Game::Systems::ProductionTasks {

namespace {

constexpr float k_work_spot_arrival_distance_sq = 0.2F * 0.2F;
constexpr float k_site_arrival_distance_sq = 1.0F * 1.0F;
constexpr float k_site_approach_limit_seconds = 30.0F;
constexpr float k_site_route_goal_tolerance_sq = 0.25F;
constexpr float k_site_progress_epsilon = 0.75F;
// A gatherer held off its exact work spot (by a crewmate, a fence post) works
// from where it stands once it is this close and has stopped gaining ground.
constexpr float k_stalled_work_reach_sq = 1.6F * 1.6F;
constexpr float k_stalled_work_seconds = 2.5F;

void face_work_target(Engine::Core::TransformComponent& transform,
                      const Engine::Core::BuilderProductionComponent& builder) {
  float target_x = 0.0F;
  float target_z = 0.0F;
  if (builder.has_task_target || builder.structure_task_entity_id != 0) {
    target_x = builder.task_target_x;
    target_z = builder.task_target_z;
  } else if (builder.has_construction_site) {
    target_x = builder.construction_site_x;
    target_z = builder.construction_site_z;
  } else {
    return;
  }

  float const dx = target_x - transform.position.x;
  float const dz = target_z - transform.position.z;
  if ((dx * dx) + (dz * dz) < 0.01F) {
    return;
  }

  transform.desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  transform.has_desired_yaw = true;
}

auto site_bypass_radius_sq(const Engine::Core::BuilderProductionComponent& builder,
                           const Engine::Core::MovementComponent* movement) -> float {
  float const radius =
      movement != nullptr && is_gather_builder_product(builder.product_type)
          ? gather_bypass_reach(movement->get_navigation_clearance())
          : k_site_bypass_reach;
  return radius * radius;
}

void activate_bypass_movement(Engine::Core::BuilderProductionComponent* builder,
                              float target_x,
                              float target_z) {
  if (builder == nullptr) {
    return;
  }
  builder->bypass_movement_active = true;
  builder->bypass_target_x = target_x;
  builder->bypass_target_z = target_z;
}

auto walking_to_site(const Engine::Core::BuilderProductionComponent& builder,
                     const Engine::Core::MovementComponent& movement) -> bool {
  if (!movement.get_has_target()) {
    return false;
  }
  const float goal_x = movement.get_has_requested_goal()
                           ? movement.get_requested_goal_x()
                           : movement.get_goal_x();
  const float goal_z = movement.get_has_requested_goal()
                           ? movement.get_requested_goal_z()
                           : movement.get_goal_y();
  const float dx = goal_x - builder.construction_site_x;
  const float dz = goal_z - builder.construction_site_z;

  float const tolerance_sq = is_gather_builder_product(builder.product_type)
                                 ? site_bypass_radius_sq(builder, &movement)
                                 : k_site_route_goal_tolerance_sq;
  return (dx * dx + dz * dz) <= tolerance_sq;
}

auto needs_site_route(const Engine::Core::BuilderProductionComponent& builder,
                      const Engine::Core::MovementComponent* movement) -> bool {
  return movement != nullptr && !walking_to_site(builder, *movement);
}

auto walkable_from_to(const Pathfinding* ground,
                      const Engine::Core::TransformComponent& from,
                      float to_x,
                      float to_z) -> bool {
  return ground == nullptr || ground->is_terrain_segment_walkable(
                                  QVector3D(from.position.x, 0.0F, from.position.z),
                                  QVector3D(to_x, 0.0F, to_z));
}

void publish_construction_started(
    const SiteApproachActor& actor,
    const Engine::Core::BuilderProductionComponent& builder) {
  Engine::Core::AudioCueEvent started = Engine::Core::AudioCueEvent::for_owner(
      actor.owner_id, "build.construction_started");
  started.at(builder.construction_site_x,
             actor.transform->position.y,
             builder.construction_site_z);
  Engine::Core::EventManager::instance().publish(started);
}

void arrive_at_site(Engine::Core::World& world,
                    const SiteApproachActor& actor,
                    Engine::Core::BuilderProductionComponent& builder,
                    const Pathfinding* ground,
                    bool work_spot,
                    bool straight_on_terrain) {
  auto& transform = *actor.transform;
  builder.at_construction_site = true;
  builder.in_progress = true;
  builder.bypass_movement_active = false;
  builder.clear_fault();
  builder.has_site_approach = true;
  builder.site_approach_x = transform.position.x;
  builder.site_approach_z = transform.position.z;
  if (!work_spot) {
    publish_construction_started(actor, builder);
  }

  if (builder.product_type == k_builder_product_harvest_grain) {
    if (auto const* field = world.try_get<Engine::Core::TransformComponent>(
            builder.structure_task_entity_id);
        field != nullptr &&
        walkable_from_to(ground, transform, field->position.x, field->position.z)) {
      builder.construction_site_x = field->position.x;
      builder.construction_site_z = field->position.z;
    }
  }
  if (straight_on_terrain) {
    transform.position.x = builder.construction_site_x;
    transform.position.z = builder.construction_site_z;
  }

  if (actor.movement != nullptr) {
    actor.movement->set_rest_position(transform.position.x, transform.position.z);
    actor.movement->stop();
  }

  face_work_target(transform, builder);
  reset_site_approach(builder);
}

void record_approach_progress(Engine::Core::World& world,
                              const SiteApproachActor& actor,
                              Engine::Core::BuilderProductionComponent& builder,
                              float dist_sq,
                              float delta_time) {
  float const distance = std::sqrt(dist_sq);

  auto const* facts = world.try_get<Engine::Core::MovementFactsComponent>(actor.id);
  bool const on_route = actor.movement != nullptr && actor.movement->get_has_target() &&
                        facts != nullptr && facts->progress.remaining_arclength > 0.0F;
  float const approach = on_route ? facts->progress.remaining_arclength : distance;
  if (builder.site_closest_approach <= 0.0F ||
      approach < builder.site_closest_approach - k_site_progress_epsilon) {
    builder.site_closest_approach = approach;
    builder.site_approach_seconds = 0.0F;
  }
  builder.site_approach_seconds += delta_time;
}

void steer_toward_site(Engine::Core::World& world,
                       const SiteApproachActor& actor,
                       Engine::Core::BuilderProductionComponent& builder,
                       float dist_sq) {
  if (dist_sq > site_bypass_radius_sq(builder, actor.movement)) {
    builder.bypass_movement_active = false;
    if (needs_site_route(builder, actor.movement)) {
      CommandService::move_unit(
          world,
          actor.id,
          QVector3D(builder.construction_site_x, 0.0F, builder.construction_site_z),
          CommandService::MoveOptions{.kind = MoveOrderKind::RecoveryMove,
                                      .preserve_formation_mode = true});
    }
  } else if (!builder.bypass_movement_active) {
    activate_bypass_movement(
        &builder, builder.construction_site_x, builder.construction_site_z);
  }
}

void give_up_on_site(Engine::Core::World& world,
                     const SiteApproachActor& actor,
                     Engine::Core::BuilderProductionComponent& builder) {
  abandon_site_route(builder, actor.movement);
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.bypass_movement_active = false;
  reset_site_approach(builder);
  clear_builder_task_target(world, &builder);
  builder.report_fault(Engine::Core::BuilderTaskFault::Unreachable);
}

} // namespace

auto distance_to_site_edge(const Engine::Core::BuilderProductionComponent& builder,
                           float x,
                           float z) -> float {
  auto const size = BuildingCollisionRegistry::get_building_size(builder.product_type);
  float const yaw =
      builder.construction_site_rotation_y * std::numbers::pi_v<float> / 180.0F;
  float const cosine = std::cos(yaw);
  float const sine = std::sin(yaw);
  float const dx = x - builder.construction_site_x;
  float const dz = z - builder.construction_site_z;
  float const local_x = std::fabs((dx * cosine) - (dz * sine));
  float const local_z = std::fabs((dx * sine) + (dz * cosine));
  float const outside_x = std::max(0.0F, local_x - (size.width * 0.5F));
  float const outside_z = std::max(0.0F, local_z - (size.depth * 0.5F));
  return std::hypot(outside_x, outside_z);
}

void reset_site_approach(Engine::Core::BuilderProductionComponent& builder) {
  builder.site_approach_seconds = 0.0F;
  builder.site_closest_approach = 0.0F;
}

void abandon_site_route(const Engine::Core::BuilderProductionComponent& builder,
                        Engine::Core::MovementComponent* movement) {
  if (movement != nullptr && walking_to_site(builder, *movement)) {
    movement->stop();
  }
}

void advance_site_approach(Engine::Core::World& world,
                           const SiteApproachActor& actor,
                           Engine::Core::BuilderProductionComponent& builder,
                           float delta_time) {
  auto& transform = *actor.transform;
  float const dx = builder.construction_site_x - transform.position.x;
  float const dz = builder.construction_site_z - transform.position.z;
  float const dist_sq = dx * dx + dz * dz;

  bool const work_spot = is_gather_builder_product(builder.product_type);
  const float arrival_sq =
      work_spot ? k_work_spot_arrival_distance_sq : k_site_arrival_distance_sq;
  float const edge =
      distance_to_site_edge(builder, transform.position.x, transform.position.z);
  bool const reached_footprint = !work_spot && (edge * edge) < arrival_sq;

  auto const* ground = NavGrid::get_pathfinder();
  bool const straight_on_terrain = walkable_from_to(
      ground, transform, builder.construction_site_x, builder.construction_site_z);
  bool const works_from_here =
      !straight_on_terrain && dist_sq <= site_bypass_radius_sq(builder, actor.movement);
  // Food tasks must reach the sheep or field; only resource gatherers can
  // work from a nearby spot when their approach stalls.
  bool const stalled_within_reach =
      is_harvest_builder_product(builder.product_type) &&
      dist_sq <= k_stalled_work_reach_sq &&
      builder.site_approach_seconds > k_stalled_work_seconds;
  if (dist_sq < arrival_sq || reached_footprint || works_from_here ||
      stalled_within_reach) {
    arrive_at_site(world,
                   actor,
                   builder,
                   ground,
                   work_spot,
                   straight_on_terrain && !stalled_within_reach);
    return;
  }

  record_approach_progress(world, actor, builder, dist_sq, delta_time);
  steer_toward_site(world, actor, builder, dist_sq);
  if (builder.site_approach_seconds > k_site_approach_limit_seconds) {
    give_up_on_site(world, actor, builder);
  }
}

} // namespace Game::Systems::ProductionTasks
