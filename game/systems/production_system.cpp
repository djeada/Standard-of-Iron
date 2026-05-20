#include "production_system.h"

#include <qvectornd.h>

#include <cmath>
#include <limits>

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../map/terrain_service.h"
#include "../units/factory.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "nation_registry.h"
#include "pathfinding.h"
#include "player_resource_registry.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"
#include "units/unit.h"
#include "wall_network_service.h"

namespace Game::Systems {

namespace {

constexpr auto k_cut_tree_product_type = "cut_tree";
constexpr auto k_collect_stone_product_type = "collect_stone";
constexpr auto k_collect_iron_ore_product_type = "collect_iron_ore";
constexpr int k_cut_tree_wood_reward = 25;
constexpr int k_collect_stone_reward = 25;
constexpr int k_collect_iron_ore_reward = 25;

void apply_production_profile(Engine::Core::ProductionComponent* prod,
                              Game::Systems::NationID nation_id,
                              Game::Units::TroopType troop_type) {
  if (prod == nullptr) {
    return;
  }
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, troop_type);
  prod->build_time = profile.production.build_time;
  prod->villager_cost = profile.production.cost;
}

auto resolve_nation_id(int owner_id) -> Game::Systems::NationID {
  auto& registry = NationRegistry::instance();
  if (const auto* nation = registry.get_nation_for_player(owner_id)) {
    return nation->id;
  }
  return registry.default_nation_id();
}

auto production_count_increment(const Engine::Core::UnitComponent* unit_comp,
                                int production_cost) -> int {
  if (unit_comp != nullptr && unit_comp->spawn_type == Game::Units::SpawnType::Home) {
    return 1;
  }
  return production_cost;
}

auto compute_builder_exit_position(float center_x,
                                   float center_z,
                                   const QVector3D& builder_pos,
                                   float unit_radius,
                                   const std::string& building_type) -> QVector3D {
  auto const size = BuildingCollisionRegistry::get_building_size(building_type);
  float const half_width = size.width * 0.5F;
  float const half_depth = size.depth * 0.5F;
  float const clearance = unit_radius + 0.25F;

  float dir_x = builder_pos.x() - center_x;
  float dir_z = builder_pos.z() - center_z;
  float const len_sq = dir_x * dir_x + dir_z * dir_z;
  if (len_sq < 0.0001F) {
    dir_x = 1.0F;
    dir_z = 0.0F;
  } else {
    float const len = std::sqrt(len_sq);
    dir_x /= len;
    dir_z /= len;
  }

  float const abs_x = std::fabs(dir_x);
  float const abs_z = std::fabs(dir_z);
  float const sx = (abs_x > 0.0001F) ? (half_width + clearance) / abs_x
                                     : std::numeric_limits<float>::infinity();
  float const sz = (abs_z > 0.0001F) ? (half_depth + clearance) / abs_z
                                     : std::numeric_limits<float>::infinity();
  float const scale = std::min(sx, sz);
  float const fallback_scale = std::max(half_width, half_depth) + clearance;
  float const final_scale =
      std::isfinite(scale) && scale > 0.0F ? scale : fallback_scale;

  return {
      center_x + dir_x * final_scale, builder_pos.y(), center_z + dir_z * final_scale};
}

auto find_guaranteed_valid_exit(float exit_x,
                                float exit_z,
                                float unit_radius) -> QVector3D {
  Pathfinding* pathfinder = CommandService::get_pathfinder();
  if (pathfinder == nullptr) {
    return {exit_x, 0.0F, exit_z};
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  Point const exit_grid = CommandService::world_to_grid(exit_x, exit_z);

  bool is_valid =
      pathfinder->is_walkable_with_radius(exit_grid.x, exit_grid.y, unit_radius);
  if (is_valid && terrain_service.is_initialized()) {
    is_valid = terrain_service.is_walkable(exit_grid.x, exit_grid.y);
  }

  if (is_valid) {
    return {exit_x, 0.0F, exit_z};
  }

  constexpr int k_max_search_radius = 50;
  Point safe_grid = exit_grid;

  for (int radius = 1; radius <= k_max_search_radius; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) {
          continue;
        }

        int const check_x = exit_grid.x + dx;
        int const check_y = exit_grid.y + dy;

        bool valid = pathfinder->is_walkable_with_radius(check_x, check_y, unit_radius);
        if (valid && terrain_service.is_initialized()) {
          valid = terrain_service.is_walkable(check_x, check_y);
        }

        if (valid) {
          safe_grid = {check_x, check_y};
          return CommandService::grid_to_world(safe_grid);
        }
      }
    }
  }

  return CommandService::grid_to_world(safe_grid);
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

void clear_builder_task_target(Engine::Core::BuilderProductionComponent* builder,
                               bool release_tree = true) {
  if (builder == nullptr) {
    return;
  }
  if (release_tree && builder->task_target_reserved) {
    Game::Map::TerrainService::instance().release_world_prop(builder->task_target_id);
  }
  builder->has_task_target = false;
  builder->task_target_id = 0;
  builder->task_target_x = 0.0F;
  builder->task_target_z = 0.0F;
  builder->task_target_reserved = false;
}

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
            ? site_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto* site =
        site_entity != nullptr
            ? site_entity->get_component<Engine::Core::WallConstructionSiteComponent>()
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
            builder_entity->get_component<Engine::Core::MovementComponent>()) {
      movement->goal_x = builder->construction_site_x;
      movement->goal_y = builder->construction_site_z;
      movement->target_x = builder->construction_site_x;
      movement->target_y = builder->construction_site_z;
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

auto skip_invalid_wall_site(Engine::Core::World* world,
                            Engine::Core::Entity* builder_entity,
                            Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (world == nullptr || builder_entity == nullptr || builder == nullptr ||
      builder->construction_site_entity_id == 0) {
    return false;
  }

  auto* site_entity = world->get_entity(builder->construction_site_entity_id);
  auto* site =
      site_entity != nullptr
          ? site_entity->get_component<Engine::Core::WallConstructionSiteComponent>()
          : nullptr;
  auto* transform = site_entity != nullptr
                        ? site_entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto* wall = site_entity != nullptr
                   ? site_entity->get_component<Engine::Core::WallSegmentComponent>()
                   : nullptr;
  if (site_entity == nullptr || site == nullptr || transform == nullptr) {
    return false;
  }

  Game::Systems::WallGridPosition const position =
      wall != nullptr ? Game::Systems::WallGridPosition{wall->grid_x, wall->grid_z}
                      : Game::Systems::WallNetworkService::snap_world_position(
                            transform->position.x, transform->position.z);
  const auto validation =
      Game::Systems::WallNetworkService::validate_wall_segment_placement(
          *world, position, true, site_entity->get_id());
  if (validation.valid) {
    return false;
  }

  PlayerResourceRegistry::instance().add(
      site->owner_id, ResourceType::Wood, WallNetworkService::k_wall_segment_wood_cost);
  world->destroy_entity(site_entity->get_id());
  builder->construction_site_entity_id = 0;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = false;
  builder->bypass_movement_active = false;
  clear_builder_task_target(builder, false);
  WallNetworkService::refresh_world(*world);
  assign_next_wall_site(world, builder_entity, builder);
  return true;
}

} // namespace

void ProductionSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto entities = world->get_entities_with<Engine::Core::ProductionComponent>();
  for (auto* e : entities) {
    auto* prod = e->get_component<Engine::Core::ProductionComponent>();
    if (prod == nullptr) {
      continue;
    }

    auto* unit_comp = e->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) && Game::Core::is_neutral_owner(unit_comp->owner_id)) {
      continue;
    }

    if (!prod->in_progress) {
      continue;
    }

    const int owner_id = (unit_comp != nullptr) ? unit_comp->owner_id : -1;
    const auto nation_id = resolve_nation_id(owner_id);
    const auto current_profile =
        TroopProfileService::instance().get_profile(nation_id, prod->product_type);
    int const individuals_per_unit = current_profile.individuals_per_unit;
    int const production_cost = current_profile.production.cost;
    int const capacity_increment =
        production_count_increment(unit_comp, production_cost);

    if ((unit_comp != nullptr) &&
        (unit_comp->spawn_type == Game::Units::SpawnType::Home) &&
        (prod->produced_count + capacity_increment > prod->max_units)) {
      prod->in_progress = false;
      continue;
    }
    prod->time_remaining -= delta_time;
    if (prod->time_remaining <= 0.0F) {

      auto* t = e->get_component<Engine::Core::TransformComponent>();
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {

        int const current_troops =
            Engine::Core::World::count_troops_for_player(u->owner_id);
        int const max_troops = Game::GameConfig::instance().get_max_troops_per_player();
        if (current_troops + production_cost > max_troops) {
          prod->in_progress = false;
          prod->time_remaining = 0.0F;
          continue;
        }

        float const exit_offset = 2.5F + 0.2F * float(prod->produced_count % 5);
        float const exit_angle = 0.5F * float(prod->produced_count % 8);
        QVector3D const raw_exit_pos =
            QVector3D(t->position.x + exit_offset * std::cos(exit_angle),
                      0.0F,
                      t->position.z + exit_offset * std::sin(exit_angle));

        auto reg = Game::Map::MapTransformer::get_factory_registry();
        if (reg) {
          Game::Units::SpawnParams sp;
          sp.player_id = u->owner_id;
          sp.spawn_type = Game::Units::spawn_typeFromTroopType(prod->product_type);
          sp.ai_controlled = e->has_component<Engine::Core::AIControlledComponent>();
          sp.nation_id = nation_id;
          sp.is_initial_spawn = false;

          float const unit_radius =
              Game::Units::TroopConfig::instance().get_selection_ring_size(
                  sp.spawn_type);
          QVector3D const safe_exit = find_guaranteed_valid_exit(
              raw_exit_pos.x(), raw_exit_pos.z(), unit_radius);
          sp.position = safe_exit;

          auto unit = reg->create(sp.spawn_type, *world, sp);

          if (unit && prod->rally_set) {
            unit->move_to(prod->rally_x, prod->rally_z);
          }
        }

        prod->produced_count += capacity_increment;
      }

      prod->in_progress = false;
      prod->time_remaining = 0.0F;

      if (!prod->production_queue.empty()) {
        prod->product_type = prod->production_queue.front();
        prod->production_queue.erase(prod->production_queue.begin());
        apply_production_profile(prod, nation_id, prod->product_type);
        prod->time_remaining = prod->build_time;
        prod->in_progress = true;
      }
    }
  }

  constexpr float CONSTRUCTION_ARRIVAL_DISTANCE_SQ = 4.0F;
  constexpr float MAX_CONSTRUCTION_DISTANCE_SQ = 9.0F;

  auto builder_entities =
      world->get_entities_with<Engine::Core::BuilderProductionComponent>();
  for (auto* e : builder_entities) {
    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod == nullptr) {
      continue;
    }

    if (builder_prod->is_placement_preview) {
      continue;
    }

    auto* transform = e->get_component<Engine::Core::TransformComponent>();
    auto* movement = e->get_component<Engine::Core::MovementComponent>();

    if (builder_prod->product_type == "wall_segment" &&
        builder_prod->construction_site_entity_id != 0 &&
        world->get_entity(builder_prod->construction_site_entity_id) == nullptr) {
      builder_prod->construction_site_entity_id = 0;
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
    }

    if (builder_prod->product_type == "wall_segment" &&
        !builder_prod->has_construction_site && !builder_prod->in_progress &&
        !builder_prod->queued_construction_site_ids.empty()) {
      assign_next_wall_site(world, e, builder_prod);
    }

    if (builder_prod->has_construction_site && !builder_prod->at_construction_site) {
      if (transform != nullptr) {
        float const dx = builder_prod->construction_site_x - transform->position.x;
        float const dz = builder_prod->construction_site_z - transform->position.z;
        float const dist_sq = dx * dx + dz * dz;

        if (dist_sq < CONSTRUCTION_ARRIVAL_DISTANCE_SQ) {

          builder_prod->at_construction_site = true;
          builder_prod->in_progress = true;
          builder_prod->bypass_movement_active = false;

          transform->position.x = builder_prod->construction_site_x;
          transform->position.z = builder_prod->construction_site_z;

          if (movement != nullptr) {
            movement->goal_x = builder_prod->construction_site_x;
            movement->goal_y = builder_prod->construction_site_z;
            movement->target_x = builder_prod->construction_site_x;
            movement->target_y = builder_prod->construction_site_z;
            movement->has_target = false;
            movement->clear_path();
            movement->vx = 0.0F;
            movement->vz = 0.0F;
          }
        } else {

          if (!builder_prod->bypass_movement_active) {
            activate_bypass_movement(builder_prod,
                                     builder_prod->construction_site_x,
                                     builder_prod->construction_site_z);
          }
        }
      }
      continue;
    }

    if (!builder_prod->in_progress) {
      continue;
    }

    if (builder_prod->at_construction_site && transform != nullptr) {
      float const dx = builder_prod->construction_site_x - transform->position.x;
      float const dz = builder_prod->construction_site_z - transform->position.z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq > MAX_CONSTRUCTION_DISTANCE_SQ) {
        builder_prod->has_construction_site = false;
        builder_prod->at_construction_site = false;
        builder_prod->in_progress = false;
        builder_prod->construction_complete = false;
        builder_prod->time_remaining = 0.0F;
        clear_builder_task_target(builder_prod);
        continue;
      }
    }

    builder_prod->time_remaining -= delta_time;
    if (builder_prod->product_type == "wall_segment" &&
        builder_prod->construction_site_entity_id != 0) {
      if (auto* site_entity =
              world->get_entity(builder_prod->construction_site_entity_id)) {
        if (auto* site =
                site_entity
                    ->get_component<Engine::Core::WallConstructionSiteComponent>()) {
          const float duration = std::max(builder_prod->build_time, 0.001F);
          site->progress =
              std::clamp(1.0F - builder_prod->time_remaining / duration, 0.0F, 1.0F);
        }
      }
    }
    if (builder_prod->time_remaining <= 0.0F) {

      auto* t = e->get_component<Engine::Core::TransformComponent>();
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {
        if (builder_prod->product_type == k_cut_tree_product_type ||
            builder_prod->product_type == k_collect_stone_product_type ||
            builder_prod->product_type == k_collect_iron_ore_product_type) {
          bool const harvested =
              builder_prod->has_task_target &&
              Game::Map::TerrainService::instance().harvest_world_prop(
                  builder_prod->task_target_id);
          if (harvested) {
            ResourceType resource_type = ResourceType::Wood;
            int reward_amount = k_cut_tree_wood_reward;
            if (builder_prod->product_type == k_collect_stone_product_type) {
              resource_type = ResourceType::Stone;
              reward_amount = k_collect_stone_reward;
            } else if (builder_prod->product_type == k_collect_iron_ore_product_type) {
              resource_type = ResourceType::Iron;
              reward_amount = k_collect_iron_ore_reward;
            }
            PlayerResourceRegistry::instance().add(
                u->owner_id, resource_type, reward_amount);
            if (auto* pathfinder = CommandService::get_pathfinder()) {
              Point const tree_grid = CommandService::world_to_grid(
                  builder_prod->task_target_x, builder_prod->task_target_z);
              pathfinder->mark_region_dirty(
                  tree_grid.x - 1, tree_grid.x + 1, tree_grid.y - 1, tree_grid.y + 1);
            }
          } else {
            clear_builder_task_target(builder_prod);
          }
        } else {
          auto reg = Game::Map::MapTransformer::get_factory_registry();
          if (reg) {
            Game::Units::SpawnParams sp;

            if (builder_prod->has_construction_site) {
              sp.position = QVector3D(builder_prod->construction_site_x,
                                      t->position.y,
                                      builder_prod->construction_site_z);
            } else {
              sp.position = QVector3D(t->position.x, t->position.y, t->position.z);
            }
            float construction_rotation_y = builder_prod->construction_site_rotation_y;
            Engine::Core::Entity* wall_site_entity = nullptr;
            if (builder_prod->product_type == "wall_segment" &&
                builder_prod->construction_site_entity_id != 0) {
              wall_site_entity =
                  world->get_entity(builder_prod->construction_site_entity_id);
              if (skip_invalid_wall_site(world, e, builder_prod)) {
                continue;
              }
              wall_site_entity =
                  world->get_entity(builder_prod->construction_site_entity_id);
              if (auto* site_transform =
                      wall_site_entity != nullptr
                          ? wall_site_entity
                                ->get_component<Engine::Core::TransformComponent>()
                          : nullptr) {
                sp.position = QVector3D(site_transform->position.x,
                                        site_transform->position.y,
                                        site_transform->position.z);
                construction_rotation_y = site_transform->rotation.y;
              }
            }
            sp.player_id = u->owner_id;
            sp.ai_controlled = e->has_component<Engine::Core::AIControlledComponent>();
            sp.nation_id = u->nation_id;
            sp.is_initial_spawn = false;
            sp.rotation_y = construction_rotation_y;

            if (builder_prod->product_type == "catapult") {
              sp.spawn_type = Game::Units::SpawnType::Catapult;
            } else if (builder_prod->product_type == "ballista") {
              sp.spawn_type = Game::Units::SpawnType::Ballista;
            } else if (builder_prod->product_type == "barracks") {
              sp.spawn_type = Game::Units::SpawnType::Barracks;
            } else if (builder_prod->product_type == "defense_tower") {
              sp.spawn_type = Game::Units::SpawnType::DefenseTower;
            } else if (builder_prod->product_type == "wall_segment") {
              sp.spawn_type = Game::Units::SpawnType::WallSegment;
            } else if (builder_prod->product_type == "home") {
              sp.spawn_type = Game::Units::SpawnType::Home;
            } else {
              builder_prod->in_progress = false;
              builder_prod->time_remaining = 0.0F;
              builder_prod->has_construction_site = false;
              builder_prod->at_construction_site = false;
              clear_builder_task_target(builder_prod, false);
              continue;
            }

            reg->create(sp.spawn_type, *world, sp);

            if (builder_prod->product_type == "wall_segment" &&
                builder_prod->construction_site_entity_id != 0) {
              world->destroy_entity(builder_prod->construction_site_entity_id);
              builder_prod->construction_site_entity_id = 0;
              WallNetworkService::refresh_world(*world);
            }

            if (builder_prod->has_construction_site && movement != nullptr &&
                t != nullptr) {
              float const unit_radius =
                  CommandService::get_unit_radius(*world, e->get_id());
              QVector3D const preferred_exit = compute_builder_exit_position(
                  builder_prod->construction_site_x,
                  builder_prod->construction_site_z,
                  QVector3D(t->position.x, t->position.y, t->position.z),
                  unit_radius,
                  builder_prod->product_type);

              QVector3D const safe_exit = find_guaranteed_valid_exit(
                  preferred_exit.x(), preferred_exit.z(), unit_radius);

              activate_bypass_movement(builder_prod, safe_exit.x(), safe_exit.z());

              movement->goal_x = safe_exit.x();
              movement->goal_y = safe_exit.z();
              movement->target_x = safe_exit.x();
              movement->target_y = safe_exit.z();
            }
          }
        }
      }

      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->construction_complete = true;
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->construction_site_entity_id = 0;
      clear_builder_task_target(builder_prod, false);
    }
  }
}

} // namespace Game::Systems
