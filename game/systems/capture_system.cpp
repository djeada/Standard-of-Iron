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

void CaptureSystem::update(Engine::Core::World *world, float deltaTime) {
  processBarrackCapture(world, deltaTime);
}

auto CaptureSystem::countNearbyTroops(Engine::Core::World *world,
                                      float barrack_x, float barrack_z,
                                      int owner_id, float radius) -> int {
  int total_troops = 0;
  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();

  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    auto *transform = e->getComponent<Engine::Core::TransformComponent>();

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
      int const individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->spawn_type);
      total_troops += individuals_per_unit;
    }
  }

  return total_troops;
}

void CaptureSystem::transferBarrackOwnership(Engine::Core::World *,
                                             Engine::Core::Entity *barrack,
                                             int newOwnerId) {
  auto *unit = barrack->getComponent<Engine::Core::UnitComponent>();
  auto *renderable = barrack->getComponent<Engine::Core::RenderableComponent>();
  auto *transform = barrack->getComponent<Engine::Core::TransformComponent>();
  auto *prod = barrack->getComponent<Engine::Core::ProductionComponent>();

  if ((unit == nullptr) || (renderable == nullptr) || (transform == nullptr)) {
    return;
  }

  int const previous_owner_id = unit->owner_id;
  unit->owner_id = newOwnerId;

  auto &nation_registry = NationRegistry::instance();
  if (!Game::Core::isNeutralOwner(newOwnerId)) {
    if (const auto *nation = nation_registry.getNationForPlayer(newOwnerId)) {
      unit->nation_id = nation->id;
    } else {
      unit->nation_id = nation_registry.default_nation_id();
    }
  } else {
    unit->nation_id.clear();
  }

  QVector3D const tc = Game::Visuals::team_colorForOwner(newOwnerId);
  renderable->color[0] = tc.x();
  renderable->color[1] = tc.y();
  renderable->color[2] = tc.z();

  Game::Systems::BuildingCollisionRegistry::instance().updateBuildingOwner(
      barrack->getId(), newOwnerId);

  if (!Game::Core::isNeutralOwner(newOwnerId) && (prod == nullptr)) {
    prod = barrack->addComponent<Engine::Core::ProductionComponent>();
    if (prod != nullptr) {
      prod->product_type = Game::Units::TroopType::Archer;
      prod->maxUnits = 150;
      prod->inProgress = false;
      prod->timeRemaining = 0.0F;
      prod->producedCount = 0;
      prod->rallyX = transform->position.x + 4.0F;
      prod->rallyZ = transform->position.z + 2.0F;
      prod->rallySet = true;
      const auto profile = TroopProfileService::instance().get_profile(
          unit->nation_id, prod->product_type);
      prod->buildTime = profile.production.build_time;
      prod->villagerCost = profile.individuals_per_unit;
    }
  } else if (Game::Core::isNeutralOwner(newOwnerId) && (prod != nullptr)) {
    barrack->removeComponent<Engine::Core::ProductionComponent>();
  } else if (prod != nullptr) {
    const auto profile = TroopProfileService::instance().get_profile(
        unit->nation_id, prod->product_type);
    prod->buildTime = profile.production.build_time;
    prod->villagerCost = profile.individuals_per_unit;
  }

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(barrack->getId(), previous_owner_id,
                                         newOwnerId));
}

void CaptureSystem::processBarrackCapture(Engine::Core::World *world,
                                          float deltaTime) {
  constexpr float capture_radius = 8.0F;
  constexpr int troop_advantage_multiplier = 3;

  auto barracks = world->getEntitiesWith<Engine::Core::BuildingComponent>();

  for (auto *barrack : barracks) {
    auto *unit = barrack->getComponent<Engine::Core::UnitComponent>();
    auto *transform = barrack->getComponent<Engine::Core::TransformComponent>();

    if ((unit == nullptr) || (transform == nullptr)) {
      continue;
    }

    if (unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *capture = barrack->getComponent<Engine::Core::CaptureComponent>();
    if (capture == nullptr) {
      capture = barrack->addComponent<Engine::Core::CaptureComponent>();
    }

    float const barrack_x = transform->position.x;
    float const barrack_z = transform->position.z;
    int const barrack_owner_id = unit->owner_id;

    int max_enemy_troops = 0;
    int capturing_player_id = -1;

    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    std::vector<int> player_ids;
    for (auto *e : entities) {
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
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
        capture->captureProgress = 0.0F;
      }

      capture->isBeingCaptured = true;
      capture->captureProgress += deltaTime;

      if (capture->captureProgress >= capture->requiredTime) {
        transferBarrackOwnership(world, barrack, capturing_player_id);
        capture->captureProgress = 0.0F;
        capture->isBeingCaptured = false;
        capture->capturing_player_id = -1;
      }
    } else {
      if (capture->isBeingCaptured) {
        capture->captureProgress -= deltaTime * 2.0F;
        if (capture->captureProgress <= 0.0F) {
          capture->captureProgress = 0.0F;
          capture->isBeingCaptured = false;
          capture->capturing_player_id = -1;
        }
      }
    }
  }
}

} // namespace Game::Systems
