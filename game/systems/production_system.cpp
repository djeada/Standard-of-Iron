#include "production_system.h"
#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "nation_registry.h"
#include "pathfinding.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"
#include "units/unit.h"
#include <cmath>
#include <limits>
#include <qvectornd.h>

namespace Game::Systems {

namespace {

void apply_production_profile(Engine::Core::ProductionComponent *prod,
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

auto resolve_nation_id(const Engine::Core::UnitComponent *unit,
                       int owner_id) -> Game::Systems::NationID {
  auto &registry = NationRegistry::instance();
  if (const auto *nation = registry.get_nation_for_player(owner_id)) {
    return nation->id;
  }
  return registry.default_nation_id();
}

auto compute_builder_exit_position(
    float center_x, float center_z, const QVector3D &builder_pos,
    float unit_radius, const std::string &building_type) -> QVector3D {
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

  return {center_x + dir_x * final_scale, builder_pos.y(),
          center_z + dir_z * final_scale};
}

auto find_guaranteed_valid_exit(float exit_x, float exit_z,
                                float unit_radius) -> QVector3D {
  Pathfinding *pathfinder = CommandService::get_pathfinder();
  if (pathfinder == nullptr) {
    return {exit_x, 0.0F, exit_z};
  }

  Point const exit_grid = CommandService::world_to_grid(exit_x, exit_z);

  if (pathfinder->is_walkable_with_radius(exit_grid.x, exit_grid.y,
                                          unit_radius)) {
    return {exit_x, 0.0F, exit_z};
  }

  constexpr int kMaxSearchRadius = 50;
  Point const safe_grid = Pathfinding::find_nearest_walkable_point(
      exit_grid, kMaxSearchRadius, *pathfinder, unit_radius);

  return CommandService::grid_to_world(safe_grid);
}

void activate_bypass_movement(Engine::Core::BuilderProductionComponent *builder,
                              float target_x, float target_z) {
  if (builder == nullptr) {
    return;
  }
  builder->bypass_movement_active = true;
  builder->bypass_target_x = target_x;
  builder->bypass_target_z = target_z;
}

} // namespace

void ProductionSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto entities = world->get_entities_with<Engine::Core::ProductionComponent>();
  for (auto *e : entities) {
    auto *prod = e->get_component<Engine::Core::ProductionComponent>();
    if (prod == nullptr) {
      continue;
    }

    auto *unit_comp = e->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) &&
        Game::Core::isNeutralOwner(unit_comp->owner_id)) {
      continue;
    }

    if (!prod->in_progress) {
      continue;
    }

    const int owner_id = (unit_comp != nullptr) ? unit_comp->owner_id : -1;
    const auto nation_id = resolve_nation_id(unit_comp, owner_id);
    const auto current_profile = TroopProfileService::instance().get_profile(
        nation_id, prod->product_type);
    int const individuals_per_unit = current_profile.individuals_per_unit;
    int const production_cost = current_profile.production.cost;

    if (prod->produced_count + production_cost > prod->max_units) {
      prod->in_progress = false;
      continue;
    }
    prod->time_remaining -= delta_time;
    if (prod->time_remaining <= 0.0F) {

      auto *t = e->get_component<Engine::Core::TransformComponent>();
      auto *u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {

        int const current_troops =
            Engine::Core::World::count_troops_for_player(u->owner_id);
        int const max_troops =
            Game::GameConfig::instance().get_max_troops_per_player();
        if (current_troops + production_cost > max_troops) {
          prod->in_progress = false;
          prod->time_remaining = 0.0F;
          continue;
        }

        float const exit_offset = 2.5F + 0.2F * float(prod->produced_count % 5);
        float const exit_angle = 0.5F * float(prod->produced_count % 8);
        QVector3D const exit_pos =
            QVector3D(t->position.x + exit_offset * std::cos(exit_angle), 0.0F,
                      t->position.z + exit_offset * std::sin(exit_angle));

        auto reg = Game::Map::MapTransformer::getFactoryRegistry();
        if (reg) {
          Game::Units::SpawnParams sp;
          sp.position = exit_pos;
          sp.player_id = u->owner_id;
          sp.spawn_type =
              Game::Units::spawn_typeFromTroopType(prod->product_type);
          sp.ai_controlled =
              e->has_component<Engine::Core::AIControlledComponent>();
          sp.nation_id = nation_id;
          auto unit = reg->create(sp.spawn_type, *world, sp);

          if (unit && prod->rally_set) {
            unit->move_to(prod->rally_x, prod->rally_z);
          }
        }

        prod->produced_count += production_cost;
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
  for (auto *e : builder_entities) {
    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod == nullptr) {
      continue;
    }

    if (builder_prod->is_placement_preview) {
      continue;
    }

    auto *transform = e->get_component<Engine::Core::TransformComponent>();
    auto *movement = e->get_component<Engine::Core::MovementComponent>();

    if (builder_prod->has_construction_site &&
        !builder_prod->at_construction_site) {
      if (transform != nullptr) {
        float dx = builder_prod->construction_site_x - transform->position.x;
        float dz = builder_prod->construction_site_z - transform->position.z;
        float dist_sq = dx * dx + dz * dz;

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
      float dx = builder_prod->construction_site_x - transform->position.x;
      float dz = builder_prod->construction_site_z - transform->position.z;
      float dist_sq = dx * dx + dz * dz;

      if (dist_sq > MAX_CONSTRUCTION_DISTANCE_SQ) {
        builder_prod->has_construction_site = false;
        builder_prod->at_construction_site = false;
        builder_prod->in_progress = false;
        builder_prod->construction_complete = false;
        builder_prod->time_remaining = 0.0F;
        continue;
      }
    }

    builder_prod->time_remaining -= delta_time;
    if (builder_prod->time_remaining <= 0.0F) {

      auto *t = e->get_component<Engine::Core::TransformComponent>();
      auto *u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {
        auto reg = Game::Map::MapTransformer::getFactoryRegistry();
        if (reg) {
          Game::Units::SpawnParams sp;

          if (builder_prod->has_construction_site) {
            sp.position =
                QVector3D(builder_prod->construction_site_x, t->position.y,
                          builder_prod->construction_site_z);
          } else {
            sp.position =
                QVector3D(t->position.x, t->position.y, t->position.z);
          }
          sp.player_id = u->owner_id;
          sp.ai_controlled =
              e->has_component<Engine::Core::AIControlledComponent>();
          sp.nation_id = u->nation_id;

          if (builder_prod->product_type == "catapult") {
            sp.spawn_type = Game::Units::SpawnType::Catapult;
          } else if (builder_prod->product_type == "ballista") {
            sp.spawn_type = Game::Units::SpawnType::Ballista;
          } else if (builder_prod->product_type == "defense_tower") {
            sp.spawn_type = Game::Units::SpawnType::DefenseTower;
          } else if (builder_prod->product_type == "home") {
            sp.spawn_type = Game::Units::SpawnType::Home;
          } else {

            builder_prod->in_progress = false;
            builder_prod->time_remaining = 0.0F;
            builder_prod->has_construction_site = false;
            builder_prod->at_construction_site = false;
            continue;
          }

          reg->create(sp.spawn_type, *world, sp);

          if (builder_prod->has_construction_site && movement != nullptr &&
              t != nullptr) {
            float const unit_radius =
                CommandService::get_unit_radius(*world, e->get_id());
            QVector3D const preferred_exit = compute_builder_exit_position(
                builder_prod->construction_site_x,
                builder_prod->construction_site_z,
                QVector3D(t->position.x, t->position.y, t->position.z),
                unit_radius, builder_prod->product_type);

            QVector3D const safe_exit = find_guaranteed_valid_exit(
                preferred_exit.x(), preferred_exit.z(), unit_radius);

            activate_bypass_movement(builder_prod, safe_exit.x(),
                                     safe_exit.z());

            movement->goal_x = safe_exit.x();
            movement->goal_y = safe_exit.z();
            movement->target_x = safe_exit.x();
            movement->target_y = safe_exit.z();
          }
        }
      }

      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->construction_complete = true;
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
    }
  }
}

} // namespace Game::Systems
