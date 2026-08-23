#include "capture_system.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/nation_registry.h"
#include "../systems/troop_profile_service.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems {

void CaptureSystem::update(Engine::Core::World* world, float delta_time) {
  process_barrack_capture(world, delta_time);
}

void CaptureSystem::tally_nearby_troops(Engine::Core::World& world,
                                        float barrack_x,
                                        float barrack_z,
                                        float radius,
                                        std::vector<OwnerTroopTally>& out) {
  out.clear();
  float const radius_sq = radius * radius;

  for (auto [entity_id, unit_ref, transform_ref] :
       world.view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
    (void)entity_id;
    const auto* unit = &unit_ref;
    const auto* transform = &transform_ref;

    if (unit->health <= 0) {
      continue;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    float const dx = transform->position.x - barrack_x;
    float const dz = transform->position.z - barrack_z;
    if ((dx * dx) + (dz * dz) > radius_sq) {
      continue;
    }

    int const production_cost =
        Game::Units::TroopConfig::instance().get_production_cost(unit->spawn_type);

    auto tally = std::find_if(
        out.begin(), out.end(), [owner_id = unit->owner_id](const OwnerTroopTally& t) {
          return t.owner_id == owner_id;
        });
    if (tally == out.end()) {
      out.push_back({unit->owner_id, production_cost});
      continue;
    }
    tally->troops += production_cost;
  }
}

void CaptureSystem::transfer_barrack_ownership(Engine::Core::World* world,
                                               Engine::Core::Entity* barrack,
                                               int new_owner_id) {
  auto* unit = barrack->get_component<Engine::Core::UnitComponent>();
  auto* renderable = barrack->get_component<Engine::Core::RenderableComponent>();
  auto* transform = barrack->get_component<Engine::Core::TransformComponent>();
  auto* prod = barrack->get_component<Engine::Core::ProductionComponent>();

  if ((unit == nullptr) || (renderable == nullptr) || (transform == nullptr)) {
    return;
  }

  int const previous_owner_id = unit->owner_id;
  unit->owner_id = new_owner_id;

  Game::Session::services_for(*world).building_collision->update_building_owner(
      barrack->get_id(), new_owner_id);

  if (!Game::Core::is_neutral_owner(new_owner_id) && (prod == nullptr)) {
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
  } else if (Game::Core::is_neutral_owner(new_owner_id) && (prod != nullptr)) {
    barrack->remove_component<Engine::Core::ProductionComponent>();
  } else if (prod != nullptr) {
    prod->produced_count = 0;
    const auto profile = TroopProfileService::instance().get_profile(
        unit->nation_id, prod->product_type);
    prod->build_time = profile.production.build_time;
    prod->villager_cost = profile.production.cost;
  }

  Engine::Core::EventManager::instance().publish(Engine::Core::BarrackCapturedEvent(
      barrack->get_id(), previous_owner_id, new_owner_id));
}

void CaptureSystem::process_barrack_capture(Engine::Core::World* world,
                                            float delta_time) {
  constexpr float capture_radius = 8.0F;
  constexpr int troop_advantage_multiplier = 3;

  if (world->entities_with<Engine::Core::BuildingComponent>().empty()) {
    return;
  }

  std::vector<OwnerTroopTally> tallies;

  for (auto [barrack_ref, building, unit_ref, transform_ref] :
       world->entity_view<Engine::Core::BuildingComponent,
                          Engine::Core::UnitComponent,
                          Engine::Core::TransformComponent>()) {
    (void)building;
    Engine::Core::Entity* barrack = &barrack_ref;
    const auto* unit = &unit_ref;
    const auto* transform = &transform_ref;

    if (unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto* capture =
        Engine::Core::get_or_add_component<Engine::Core::CaptureComponent>(*barrack);
    if (capture == nullptr) {
      continue;
    }

    float const barrack_x = transform->position.x;
    float const barrack_z = transform->position.z;
    int const barrack_owner_id = unit->owner_id;

    int max_enemy_troops = 0;
    int capturing_player_id = -1;

    tally_nearby_troops(*world, barrack_x, barrack_z, capture_radius, tallies);

    int defender_troops = 0;
    for (const auto& tally : tallies) {
      if (tally.owner_id == barrack_owner_id) {
        if (!Game::Core::is_neutral_owner(barrack_owner_id)) {
          defender_troops = tally.troops;
        }
        continue;
      }
      if (Game::Core::is_neutral_owner(tally.owner_id)) {
        continue;
      }
      if (tally.troops > max_enemy_troops) {
        max_enemy_troops = tally.troops;
        capturing_player_id = tally.owner_id;
      }
    }

    bool const can_capture =
        !capture->capture_blocked &&
        max_enemy_troops >= (defender_troops * troop_advantage_multiplier);

    if (can_capture && capturing_player_id != -1) {
      if (capture->capturing_player_id != capturing_player_id) {
        capture->capturing_player_id = capturing_player_id;
        capture->capture_progress = 0.0F;
      }

      capture->is_being_captured = true;
      capture->capture_progress += delta_time;

      if (capture->capture_progress >= capture->required_time) {
        transfer_barrack_ownership(world, barrack, capturing_player_id);
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

auto CaptureSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<TransformComponent, BuildingComponent>{},
                               Writes<CaptureComponent,
                                      ProductionComponent,
                                      RenderableComponent,
                                      UnitComponent>{});
}

} // namespace Game::Systems
