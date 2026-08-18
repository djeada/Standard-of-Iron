#include "food_targets.h"

#include <cmath>
#include <limits>

#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "builder_product_types.h"
#include "command_service.h"
#include "construction_cost_catalog.h"
#include "nav_grid.h"

namespace Game::Systems {

namespace {

auto entity_alive(const Engine::Core::Entity& entity,
                  const Engine::Core::UnitComponent& unit) -> bool {
  return unit.health > 0 &&
         !entity.has_component<Engine::Core::PendingRemovalComponent>() &&
         !entity.has_component<Engine::Core::DeathAnimationComponent>();
}

auto distance_sq(float ax, float az, float bx, float bz) -> float {
  float const dx = ax - bx;
  float const dz = az - bz;
  return dx * dx + dz * dz;
}

} // namespace

auto farm_is_harvestable(const Engine::Core::Entity& farm, int owner_id) -> bool {
  const auto* unit = farm.get_component<Engine::Core::UnitComponent>();
  const auto* crop = farm.get_component<Engine::Core::FarmComponent>();
  if (unit == nullptr || crop == nullptr || unit->owner_id != owner_id) {
    return false;
  }
  if (unit->spawn_type != Game::Units::SpawnType::Farm || !entity_alive(farm, *unit)) {
    return false;
  }
  if (farm.has_component<Engine::Core::DismantleSiteComponent>()) {
    return false;
  }
  return crop->ripe();
}

auto sheep_is_slaughterable(const Engine::Core::Entity& sheep) -> bool {
  const auto* unit = sheep.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Sheep) {
    return false;
  }
  return entity_alive(sheep, *unit);
}

auto food_target_claimed(Engine::Core::World& world,
                         Engine::Core::EntityID target_id,
                         Engine::Core::EntityID except_worker) -> bool {
  if (target_id == 0) {
    return false;
  }
  for (auto* worker :
       world.get_entities_with<Engine::Core::BuilderProductionComponent>()) {
    if (worker == nullptr || worker->get_id() == except_worker) {
      continue;
    }
    const auto* builder =
        worker->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder != nullptr && builder->structure_task_entity_id == target_id &&
        is_food_builder_product(builder->product_type)) {
      return true;
    }
  }
  return false;
}

auto resolve_food_target(Engine::Core::World& world,
                         Engine::Core::EntityID target_id,
                         int owner_id) -> std::optional<FoodTarget> {
  auto* entity = world.get_entity(target_id);
  if (entity == nullptr) {
    return std::nullopt;
  }
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return std::nullopt;
  }
  if (farm_is_harvestable(*entity, owner_id)) {
    return FoodTarget{.id = target_id,
                      .product_type = k_builder_product_harvest_grain,
                      .x = transform->position.x,
                      .z = transform->position.z};
  }
  if (sheep_is_slaughterable(*entity)) {
    return FoodTarget{.id = target_id,
                      .product_type = k_builder_product_slaughter_sheep,
                      .x = transform->position.x,
                      .z = transform->position.z};
  }
  return std::nullopt;
}

auto find_food_target_near(Engine::Core::World& world,
                           std::string_view product_type,
                           int owner_id,
                           float x,
                           float z,
                           float radius,
                           Engine::Core::EntityID except_worker)
    -> std::optional<FoodTarget> {
  float const radius_sq =
      radius > 0.0F ? radius * radius : std::numeric_limits<float>::infinity();
  bool const want_grain =
      product_type.empty() || product_type == k_builder_product_harvest_grain;
  bool const want_sheep =
      product_type.empty() || product_type == k_builder_product_slaughter_sheep;

  std::optional<FoodTarget> best;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  auto consider = [&](Engine::Core::Entity& entity, std::string_view product) {
    const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      return;
    }
    float const d = distance_sq(transform->position.x, transform->position.z, x, z);
    if (d > radius_sq || d >= best_distance_sq) {
      return;
    }
    if (food_target_claimed(world, entity.get_id(), except_worker)) {
      return;
    }
    best_distance_sq = d;
    best = FoodTarget{.id = entity.get_id(),
                      .product_type = product,
                      .x = transform->position.x,
                      .z = transform->position.z};
  };

  if (want_grain) {
    for (auto* entity : world.get_entities_with<Engine::Core::FarmComponent>()) {
      if (entity != nullptr && farm_is_harvestable(*entity, owner_id)) {
        consider(*entity, k_builder_product_harvest_grain);
      }
    }
  }
  if (want_sheep) {
    for (auto* entity : world.get_entities_with<Engine::Core::WildlifeComponent>()) {
      if (entity != nullptr && sheep_is_slaughterable(*entity)) {
        consider(*entity, k_builder_product_slaughter_sheep);
      }
    }
  }
  return best;
}

auto owner_has_farm_near(
    Engine::Core::World& world, int owner_id, float x, float z, float radius) -> bool {
  float const radius_sq = radius * radius;
  for (auto* entity : world.get_entities_with<Engine::Core::FarmComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->owner_id != owner_id ||
        !entity_alive(*entity, *unit)) {
      continue;
    }
    if (distance_sq(transform->position.x, transform->position.z, x, z) <= radius_sq) {
      return true;
    }
  }
  return false;
}

auto food_work_position(Engine::Core::World& world,
                        Engine::Core::EntityID worker_id,
                        const QVector3D& worker_position,
                        const FoodTarget& target) -> QVector3D {
  QVector3D const target_position(target.x, 0.0F, target.z);
  if (target.product_type == k_builder_product_harvest_grain) {
    return CommandService::structure_work_position(
        worker_position,
        target_position,
        "farm",
        CommandService::get_unit_radius(world, worker_id));
  }

  QVector3D approach = worker_position - target_position;
  approach.setY(0.0F);
  if (approach.lengthSquared() < 0.01F) {
    approach = QVector3D(1.0F, 0.0F, 0.0F);
  }
  approach.normalize();
  return NavGrid::snap_to_walkable_ground(target_position +
                                          approach * k_sheep_work_standoff);
}

void assign_food_task(Engine::Core::BuilderProductionComponent& builder,
                      Engine::Core::MovementComponent* movement,
                      const FoodTarget& target,
                      const QVector3D& work_position) {
  builder.product_type = std::string(target.product_type);
  builder.build_time = construction_build_time(target.product_type);
  builder.time_remaining = builder.build_time;
  builder.has_construction_site = true;
  builder.construction_site_x = work_position.x();
  builder.construction_site_z = work_position.z();
  builder.construction_site_rotation_y = 0.0F;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.construction_complete = false;
  builder.bypass_movement_active = false;
  builder.construction_site_entity_id = 0;
  builder.structure_task_entity_id = target.id;
  builder.has_task_target = false;
  builder.task_target_id = 0;
  builder.task_target_x = target.x;
  builder.task_target_z = target.z;
  builder.task_target_reserved = false;
  builder.clear_fault();
  if (movement != nullptr) {
    movement->set_rest_position(work_position.x(), work_position.z());
  }
}

} // namespace Game::Systems
