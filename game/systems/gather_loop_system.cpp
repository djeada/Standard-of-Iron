#include "gather_loop_system.h"

#include <QVector3D>

#include <optional>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "builder_product_types.h"
#include "command_service.h"

namespace Game::Systems {
namespace {

constexpr float k_work_standoff = 1.8F;

auto find_next_node(const std::string& product_type,
                    float anchor_x,
                    float anchor_z) -> std::optional<Game::Map::WorldPropTarget> {
  auto const& terrain = Game::Map::TerrainService::instance();
  float const radius = GatherLoopSystem::k_search_radius;

  if (product_type == k_builder_product_cut_tree) {
    return terrain.find_tree_near_world(anchor_x, anchor_z, radius);
  }
  if (product_type == k_builder_product_collect_stone) {
    return terrain.find_boulder_near_world(anchor_x, anchor_z, radius);
  }
  if (product_type == k_builder_product_collect_iron_ore) {
    return terrain.find_iron_ore_near_world(anchor_x, anchor_z, radius);
  }
  return std::nullopt;
}

auto work_position_beside(const Game::Map::WorldPropTarget& node,
                          float worker_x,
                          float worker_z) -> QVector3D {
  QVector3D approach(worker_x - node.x, 0.0F, worker_z - node.z);
  if (approach.lengthSquared() < 0.01F) {
    approach = QVector3D(1.0F, 0.0F, 0.0F);
  }
  approach.normalize();

  return CommandService::snap_to_walkable_ground(
      QVector3D(node.x + (approach.x() * k_work_standoff),
                0.0F,
                node.z + (approach.z() * k_work_standoff)));
}

auto is_free_for_the_next_load(const Engine::Core::Entity& worker,
                               const Engine::Core::BuilderProductionComponent& builder)
    -> bool {
  if (builder.in_progress || builder.has_construction_site || builder.has_task_target ||
      builder.structure_task_entity_id != 0 ||
      !builder.queued_construction_site_ids.empty()) {
    return false;
  }
  if (const auto* carry = worker.get_component<Engine::Core::ResourceCarryComponent>();
      carry != nullptr && !carry->empty()) {
    return false;
  }
  if (worker.has_component<Engine::Core::AttackTargetComponent>()) {
    return false;
  }
  const auto* movement = worker.get_component<Engine::Core::MovementComponent>();
  return movement != nullptr && !movement->get_has_target();
}

} // namespace

void GatherLoopSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_think_cooldown -= delta_time;
  if (m_think_cooldown > 0.0F) {
    return;
  }
  m_think_cooldown = k_think_interval;

  auto& terrain = Game::Map::TerrainService::instance();

  for (auto* entity :
       world->get_entities_with<Engine::Core::BuilderProductionComponent>()) {
    auto* builder = entity->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder == nullptr || !builder->has_gather_order) {
      continue;
    }

    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (builder->is_placement_preview || unit == nullptr || unit->health <= 0 ||
        !is_harvest_builder_product(builder->gather_product_type)) {
      builder->clear_gather_order();
      continue;
    }

    if (!is_free_for_the_next_load(*entity, *builder)) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (transform == nullptr || movement == nullptr) {
      continue;
    }

    auto const next = find_next_node(builder->gather_product_type,
                                     builder->gather_anchor_x,
                                     builder->gather_anchor_z);
    if (!next.has_value()) {
      builder->clear_gather_order();
      continue;
    }

    if (!terrain.reserve_world_prop(next->id)) {
      continue;
    }

    QVector3D const work_position =
        work_position_beside(*next, transform->position.x, transform->position.z);

    builder->product_type = builder->gather_product_type;
    builder->time_remaining = builder->build_time;
    builder->has_construction_site = true;
    builder->construction_site_x = work_position.x();
    builder->construction_site_z = work_position.z();
    builder->construction_site_rotation_y = 0.0F;
    builder->at_construction_site = false;
    builder->in_progress = false;
    builder->construction_complete = false;
    builder->bypass_movement_active = false;
    builder->has_task_target = true;
    builder->task_target_id = next->id;
    builder->task_target_x = next->x;
    builder->task_target_z = next->z;
    builder->task_target_reserved = true;
    builder->clear_fault();

    movement->set_rest_position(work_position.x(), work_position.z());
  }
}

} // namespace Game::Systems
