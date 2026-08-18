#include "gather_loop_system.h"

#include <QVector3D>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../map/visibility_service.h"
#include "builder_product_types.h"
#include "nav_grid.h"
#include "owner_registry.h"
#include "pathfinding.h"

namespace Game::Systems {
namespace {

constexpr float k_work_standoff = 1.8F;

constexpr int k_work_position_search_radius = 4;

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

auto harvest_product_for(Game::Map::WorldProp::Type type) -> std::string_view {
  if (Game::Map::is_tree_world_prop_type(type)) {
    return k_builder_product_cut_tree;
  }
  if (Game::Map::is_boulder_world_prop_type(type)) {
    return k_builder_product_collect_stone;
  }
  return k_builder_product_collect_iron_ore;
}

auto work_position_beside(const Game::Map::WorldPropTarget& node,
                          float worker_x,
                          float worker_z) -> QVector3D {
  QVector3D approach(worker_x - node.x, 0.0F, worker_z - node.z);
  if (approach.lengthSquared() < 0.01F) {
    approach = QVector3D(1.0F, 0.0F, 0.0F);
  }
  approach.normalize();

  return NavGrid::snap_to_walkable_ground(
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

struct Candidate {
  Game::Map::WorldPropTarget target;
  float distance_sq{0.0F};
  bool matches_priority{false};
};

auto node_is_hidden_by_fog(const Engine::Core::Entity& worker,
                           float x,
                           float z) -> bool {
  const auto* unit = worker.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr ||
      unit->owner_id != OwnerRegistry::instance().get_local_player_id()) {
    return false;
  }
  auto const& visibility = Game::Map::VisibilityService::instance();
  if (!visibility.is_initialized()) {
    return false;
  }
  return !visibility.is_explored_world(x, z);
}

auto rank_nearby_nodes(const Engine::Core::Entity& worker,
                       const Engine::Core::BuilderProductionComponent& builder,
                       float from_x,
                       float from_z,
                       float radius) -> std::vector<Candidate> {
  auto const& terrain = Game::Map::TerrainService::instance();
  std::string_view const priority = builder.auto_gather_priority;
  float const radius_sq = radius > 0.0F ? radius * radius : 0.0F;

  std::vector<Candidate> candidates;
  for (const auto& prop : terrain.world_props()) {
    if (!Game::Map::is_harvestable_world_prop_type(prop.type) ||
        terrain.is_world_prop_reserved(prop.id)) {
      continue;
    }

    auto const position = terrain.world_prop_world_position(prop);
    float const dx = position.x() - from_x;
    float const dz = position.z() - from_z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (radius_sq > 0.0F && distance_sq > radius_sq) {
      continue;
    }

    if (node_is_hidden_by_fog(worker, position.x(), position.z())) {
      continue;
    }

    candidates.push_back(Candidate{
        .target =
            {.id = prop.id, .type = prop.type, .x = position.x(), .z = position.z()},
        .distance_sq = distance_sq,
        .matches_priority =
            !priority.empty() && harvest_product_for(prop.type) == priority});
  }

  auto const closest_first = [](const Candidate& a, const Candidate& b) {
    if (a.matches_priority != b.matches_priority) {
      return a.matches_priority;
    }
    if (a.distance_sq != b.distance_sq) {
      return a.distance_sq < b.distance_sq;
    }

    return a.target.id < b.target.id;
  };

  auto const shortlist =
      std::min(candidates.size(),
               static_cast<std::size_t>(GatherLoopSystem::k_auto_gather_attempts));
  std::partial_sort(candidates.begin(),
                    candidates.begin() + static_cast<std::ptrdiff_t>(shortlist),
                    candidates.end(),
                    closest_first);
  candidates.resize(shortlist);
  return candidates;
}

auto node_is_workable(const Game::Map::WorldPropTarget& node,
                      float worker_x,
                      float worker_z) -> bool {

  Point const node_grid = NavGrid::world_to_grid(node.x, node.z);
  auto const standing_cell =
      NavGrid::find_nearest_walkable_grid(node_grid, k_work_position_search_radius);
  if (!standing_cell.has_value()) {
    return false;
  }

  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return true;
  }

  Point const start = NavGrid::world_to_grid(worker_x, worker_z);
  if (start.x == standing_cell->x && start.y == standing_cell->y) {
    return true;
  }

  if (!pathfinder->is_walkable(start.x, start.y)) {
    return true;
  }

  auto const path = pathfinder->find_path(start, *standing_cell);
  return !path.empty() && path.back().x == standing_cell->x &&
         path.back().y == standing_cell->y;
}

void assign_node(Engine::Core::BuilderProductionComponent& builder,
                 Engine::Core::MovementComponent& movement,
                 std::string_view product_type,
                 const Game::Map::WorldPropTarget& node,
                 const QVector3D& work_position) {
  builder.product_type = std::string(product_type);
  builder.time_remaining = builder.build_time;
  builder.has_construction_site = true;
  builder.construction_site_x = work_position.x();
  builder.construction_site_z = work_position.z();
  builder.construction_site_rotation_y = 0.0F;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.construction_complete = false;
  builder.bypass_movement_active = false;
  builder.has_task_target = true;
  builder.task_target_id = node.id;
  builder.task_target_x = node.x;
  builder.task_target_z = node.z;
  builder.task_target_reserved = true;
  builder.clear_fault();

  movement.set_rest_position(work_position.x(), work_position.z());
}

auto claim_from(Engine::Core::Entity& worker,
                Engine::Core::BuilderProductionComponent& builder,
                Engine::Core::TransformComponent& transform,
                Engine::Core::MovementComponent& movement,
                float radius) -> bool {
  auto& terrain = Game::Map::TerrainService::instance();
  auto const candidates = rank_nearby_nodes(
      worker, builder, transform.position.x, transform.position.z, radius);

  for (const auto& candidate : candidates) {
    if (!node_is_workable(
            candidate.target, transform.position.x, transform.position.z)) {
      continue;
    }

    QVector3D const work_position = work_position_beside(
        candidate.target, transform.position.x, transform.position.z);

    if (!terrain.reserve_world_prop(candidate.target.id)) {
      continue;
    }

    assign_node(builder,
                movement,
                harvest_product_for(candidate.target.type),
                candidate.target,
                work_position);
    return true;
  }

  return false;
}

auto take_next_auto_gather_node(Engine::Core::Entity& worker,
                                Engine::Core::BuilderProductionComponent& builder,
                                Engine::Core::TransformComponent& transform,
                                Engine::Core::MovementComponent& movement) -> bool {
  if (claim_from(
          worker, builder, transform, movement, GatherLoopSystem::k_search_radius)) {
    return true;
  }

  return claim_from(worker, builder, transform, movement, 0.0F);
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
    if (builder == nullptr || (!builder->has_gather_order && !builder->auto_gather)) {
      continue;
    }

    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      builder->clear_gather_order();
      builder->clear_auto_gather();
      continue;
    }

    if (builder->has_gather_order &&
        !is_harvest_builder_product(builder->gather_product_type)) {
      builder->clear_gather_order();
      if (!builder->auto_gather) {
        continue;
      }
    }

    if (!is_free_for_the_next_load(*entity, *builder)) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (transform == nullptr || movement == nullptr) {
      continue;
    }

    if (builder->auto_gather) {
      if (!take_next_auto_gather_node(*entity, *builder, *transform, *movement)) {

        builder->report_fault(Engine::Core::BuilderTaskFault::TargetLost,
                              k_think_interval);
      }
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

    assign_node(
        *builder, *movement, builder->gather_product_type, *next, work_position);
  }
}

} // namespace Game::Systems
