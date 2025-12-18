#include "capture_system.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/nation_registry.h"
#include "../systems/troop_profile_service.h"
#include "../units/troop_config.h"
#include "../visuals/team_colors.h"
#include "building_collision_registry.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"
#include <algorithm>
#include <cmath>
#include <qvectornd.h>
#include <vector>

namespace Game::Systems {

void CaptureSystem::update(Engine::Core::World *world, float delta_time) {
  processBarrackCapture(world, delta_time);
}

auto CaptureSystem::countNearbyTroops(Engine::Core::World *world,
                                      float barrack_x, float barrack_z,
                                      int owner_id, float radius) -> int {
  int total_troops = 0;
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    auto *transform = e->get_component<Engine::Core::TransformComponent>();

    if ((unit == nullptr) || (transform == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id != owner_id) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    float const dx = transform->position.x - barrack_x;
    float const dz = transform->position.z - barrack_z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq <= radius * radius) {
      int const production_cost =
          Game::Units::TroopConfig::instance().getProductionCost(
              unit->spawn_type);
      total_troops += production_cost;
    }
  }

  return total_troops;
}

void CaptureSystem::transferBarrackOwnership(Engine::Core::World *,
                                             Engine::Core::Entity *barrack,
                                             int new_owner_id) {
  auto *unit = barrack->get_component<Engine::Core::UnitComponent>();
  auto *renderable =
      barrack->get_component<Engine::Core::RenderableComponent>();
  auto *transform = barrack->get_component<Engine::Core::TransformComponent>();
  auto *prod = barrack->get_component<Engine::Core::ProductionComponent>();

  if ((unit == nullptr) || (renderable == nullptr) || (transform == nullptr)) {
    return;
  }

  int const previous_owner_id = unit->owner_id;
  unit->owner_id = new_owner_id;

  QVector3D const tc = Game::Visuals::team_colorForOwner(new_owner_id);
  renderable->color[0] = tc.x();
  renderable->color[1] = tc.y();
  renderable->color[2] = tc.z();

  Game::Systems::BuildingCollisionRegistry::instance().update_building_owner(
      barrack->get_id(), new_owner_id);

  if (!Game::Core::isNeutralOwner(new_owner_id) && (prod == nullptr)) {
    prod = barrack->add_component<Engine::Core::ProductionComponent>();
    if (prod != nullptr) {
      prod->product_type = Game::Units::TroopType::Archer;
      prod->max_units = 150;
      prod->in_progress = false;
      prod->time_remaining = 0.0F;
      prod->produced_count = 0;
      prod->rally_x = transform->position.x + 4.0F;
      prod->rally_z = transform->position.z + 2.0F;
      prod->rally_set = true;
      const auto profile = TroopProfileService::instance().get_profile(
          unit->nation_id, prod->product_type);
      prod->build_time = profile.production.build_time;
      prod->villager_cost = profile.production.cost;
    }
  } else if (Game::Core::isNeutralOwner(new_owner_id) && (prod != nullptr)) {
    barrack->remove_component<Engine::Core::ProductionComponent>();
  } else if (prod != nullptr) {
    const auto profile = TroopProfileService::instance().get_profile(
        unit->nation_id, prod->product_type);
    prod->build_time = profile.production.build_time;
    prod->villager_cost = profile.production.cost;
  }

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(barrack->get_id(), previous_owner_id,
                                         new_owner_id));
}

void CaptureSystem::processBarrackCapture(Engine::Core::World *world,
                                          float delta_time) {
  constexpr float capture_radius = 8.0F;
  constexpr int troop_advantage_multiplier = 3;

  auto barracks = world->get_entities_with<Engine::Core::BuildingComponent>();

  for (auto *barrack : barracks) {
    auto *unit = barrack->get_component<Engine::Core::UnitComponent>();
    auto *transform =
        barrack->get_component<Engine::Core::TransformComponent>();

    if ((unit == nullptr) || (transform == nullptr)) {
      continue;
    }

    if (unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *capture = barrack->get_component<Engine::Core::CaptureComponent>();
    if (capture == nullptr) {
      capture = barrack->add_component<Engine::Core::CaptureComponent>();
    }

    float const barrack_x = transform->position.x;
    float const barrack_z = transform->position.z;
    int const barrack_owner_id = unit->owner_id;

    int max_enemy_troops = 0;
    int capturing_player_id = -1;

    auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
    std::vector<int> player_ids;
    for (auto *e : entities) {
      auto *u = e->get_component<Engine::Core::UnitComponent>();
      if ((u != nullptr) && u->owner_id != barrack_owner_id &&
          !Game::Core::isNeutralOwner(u->owner_id)) {
        if (std::find(player_ids.begin(), player_ids.end(), u->owner_id) ==
            player_ids.end()) {
          player_ids.push_back(u->owner_id);
        }
      }
    }

    for (int const player_id : player_ids) {
      int const troop_count = countNearbyTroops(world, barrack_x, barrack_z,
                                                player_id, capture_radius);
      if (troop_count > max_enemy_troops) {
        max_enemy_troops = troop_count;
        capturing_player_id = player_id;
      }
    }

    int defender_troops = 0;
    if (!Game::Core::isNeutralOwner(barrack_owner_id)) {
      defender_troops = countNearbyTroops(world, barrack_x, barrack_z,
                                          barrack_owner_id, capture_radius);
    }

    bool const can_capture =
        max_enemy_troops >= (defender_troops * troop_advantage_multiplier);

    if (can_capture && capturing_player_id != -1) {
      if (capture->capturing_player_id != capturing_player_id) {
        capture->capturing_player_id = capturing_player_id;
        capture->capture_progress = 0.0F;
      }

      capture->is_being_captured = true;
      capture->capture_progress += delta_time;

      if (capture->capture_progress >= capture->required_time) {
        transferBarrackOwnership(world, barrack, capturing_player_id);
        capture->capture_progress = 0.0F;
        capture->is_being_captured = false;
        capture->capturing_player_id = -1;
      }
    } else {
      if (capture->is_being_captured) {
        capture->capture_progress -= delta_time * 2.0F;
        if (capture->capture_progress <= 0.0F) {
          capture->capture_progress = 0.0F;
          capture->is_being_captured = false;
          capture->capturing_player_id = -1;
        }
      }
    }
  }
}

} // namespace Game::Systems
