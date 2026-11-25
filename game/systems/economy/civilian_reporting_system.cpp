#include "civilian_reporting_system.h"

#include <QVector3D>

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "core/ambient_session.h"
#include "core/component_combat.h"
#include "core/component_core.h"
#include "core/component_economy.h"
#include "core/component_structures.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "recruitment_gate.h"
#include "systems/economy/production_service.h"
#include "systems/movement/command_service.h"
#include "systems/nation_registry.h"
#include "systems/player_feedback.h"
#include "systems/recruitment_rules.h"
#include "systems/troop_profile_service.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems {
namespace {

using Engine::Core::EntityID;

struct Barracks {
  EntityID id = 0;
  int owner_id = 0;
  QVector3D position;
  QVector3D gate;
  int free_households = 0;
};

struct Home {
  EntityID id = 0;
  int owner_id = 0;
  QVector3D position;
  bool reports = false;
};

struct Civilian {
  EntityID id = 0;
  int owner_id = 0;
  QVector3D position;
};

auto planar_distance_sq(const QVector3D& a, const QVector3D& b) -> float {
  const float dx = a.x() - b.x();
  const float dz = a.z() - b.z();
  return (dx * dx) + (dz * dz);
}

auto position_of(const Engine::Core::TransformComponent& transform) -> QVector3D {
  return {transform.position.x, 0.0F, transform.position.z};
}

auto by_id(std::vector<Engine::Core::Entity*> entities)
    -> std::vector<Engine::Core::Entity*> {
  std::sort(entities.begin(), entities.end(), [](const auto* a, const auto* b) {
    return a->get_id() < b->get_id();
  });
  return entities;
}

auto is_free_to_report(const Engine::Core::World& world, Engine::Core::EntityID id) -> bool {
  if (world.has<Engine::Core::CivilianDeliveryComponent>(id) ||
      world.has<Engine::Core::PlayerOrderIntentComponent>(id) ||
      world.has<Engine::Core::AttackTargetComponent>(id)) {
    return false;
  }
  return true;
}

// Queues one civilian at a home without the recruit fanfare a player's click
// gets: the same rules, costs and floating numbers, no audio cue per family.
auto queue_civilian(Engine::Core::World& world, EntityID home_id, int owner_id)
    -> bool {
  if (ProductionService::can_start_production(
          world, home_id, Game::Units::TroopType::Civilian) !=
      ProductionResult::Success) {
    return false;
  }
  auto* production = world.try_get<Engine::Core::ProductionComponent>(home_id);
  if (production == nullptr || production->in_progress) {
    return false;
  }
  const auto& nations = *Game::Session::services_for(world).nations;
  const auto* nation = nations.get_nation_for_player(owner_id);
  const auto nation_id = nation != nullptr ? nation->id : nations.default_nation_id();
  const auto& profile = TroopProfileService::instance().get_profile_ref(
      nation_id, Game::Units::TroopType::Civilian);

  production->product_type = Game::Units::TroopType::Civilian;
  production->build_time = profile.production.build_time;
  production->villager_cost = profile.production.cost;
  production->time_remaining = production->build_time;
  production->in_progress = true;
  spend_manpower(owner_id, home_id, *production, profile.production.cost);
  spend_resources(owner_id, home_id, profile.production.resource_costs);
  return true;
}

} // namespace

void CivilianReportingSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  m_think_cooldown -= delta_time;
  if (m_think_cooldown > 0.0F) {
    return;
  }
  m_think_cooldown = k_think_interval;

  std::vector<Barracks> barracks;
  std::vector<Home> homes;
  std::vector<Civilian> civilians;
  std::map<EntityID, int> en_route;
  std::map<int, int> households_coming;

  for (auto* entity :
       by_id(world->collect_entities_with<Engine::Core::UnitComponent>())) {
    const auto id = entity->get_id();
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(id);
    const auto* transform = world->try_get<Engine::Core::TransformComponent>(id);
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        Game::Core::is_neutral_owner(unit->owner_id)) {
      continue;
    }
    switch (unit->spawn_type) {
    case Game::Units::SpawnType::Barracks: {
      const auto* production = world->try_get<Engine::Core::ProductionComponent>(id);
      if (production == nullptr) {
        break;
      }
      const int room = std::max(0, production->manpower_limit() -
                                       production->manpower_available);
      barracks.push_back({.id = entity->get_id(),
                          .owner_id = unit->owner_id,
                          .position = position_of(*transform),
                          .gate = barracks_gate(*transform).position,
                          .free_households = room / k_civilian_delivery_reserve_grant});
      break;
    }
    case Game::Units::SpawnType::Home: {
      const auto* home = world->try_get<Engine::Core::HomeComponent>(id);
      if (home == nullptr) {
        break;
      }
      homes.push_back({.id = entity->get_id(),
                       .owner_id = unit->owner_id,
                       .position = position_of(*transform),
                       .reports = home->reports_to_barracks});
      if (const auto* production =
              world->try_get<Engine::Core::ProductionComponent>(id)) {
        households_coming[unit->owner_id] +=
            (production->in_progress ? 1 : 0) +
            static_cast<int>(production->production_queue.size());
      }
      break;
    }
    case Game::Units::SpawnType::Civilian: {
      if (const auto* delivery =
              world->try_get<Engine::Core::CivilianDeliveryComponent>(id)) {
        ++en_route[delivery->target_barracks_id];
        break;
      }
      ++households_coming[unit->owner_id];
      if (is_free_to_report(*world, id)) {
        civilians.push_back({.id = entity->get_id(),
                             .owner_id = unit->owner_id,
                             .position = position_of(*transform)});
      }
      break;
    }
    default:
      break;
    }
  }

  for (auto& hall : barracks) {
    hall.free_households = std::max(0, hall.free_households - en_route[hall.id]);
  }

  const auto reporting_home_near = [&homes](const Civilian& civilian) -> bool {
    const Home* nearest = nullptr;
    float best = k_home_reach * k_home_reach;
    for (const auto& home : homes) {
      if (home.owner_id != civilian.owner_id) {
        continue;
      }
      const float distance = planar_distance_sq(home.position, civilian.position);
      if (distance <= best) {
        best = distance;
        nearest = &home;
      }
    }
    return nearest != nullptr && nearest->reports;
  };
  const auto waits_at_a_barracks = [&barracks](const Civilian& civilian) -> bool {
    return std::any_of(barracks.begin(), barracks.end(), [&](const Barracks& hall) {
      return hall.owner_id == civilian.owner_id &&
             planar_distance_sq(hall.position, civilian.position) <=
                 k_barracks_reach * k_barracks_reach;
    });
  };

  std::vector<CommandService::MoveIntent> walks;
  for (const auto& civilian : civilians) {
    if (!reporting_home_near(civilian) && !waits_at_a_barracks(civilian)) {
      continue;
    }
    Barracks* destination = nullptr;
    float best = std::numeric_limits<float>::max();
    for (auto& hall : barracks) {
      if (hall.owner_id != civilian.owner_id || hall.free_households <= 0) {
        continue;
      }
      const float distance = planar_distance_sq(hall.position, civilian.position);
      if (distance < best) {
        best = distance;
        destination = &hall;
      }
    }
    if (destination == nullptr) {
      continue;
    }
    auto* delivery = world->emplace<Engine::Core::CivilianDeliveryComponent>(civilian.id);
    if (delivery == nullptr) {
      continue;
    }
    delivery->target_barracks_id = destination->id;
    --destination->free_households;
    walks.push_back({.unit_id = civilian.id, .target = destination->gate});
  }
  if (!walks.empty()) {
    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::ScriptedMove;
    CommandService::move_units(*world, walks, options);
  }

  std::map<int, int> free_households;
  for (const auto& hall : barracks) {
    free_households[hall.owner_id] += hall.free_households;
  }
  for (const auto& civilian : civilians) {
    if (world->has<Engine::Core::CivilianDeliveryComponent>(civilian.id)) {
      --households_coming[civilian.owner_id];
    }
  }
  for (const auto& home : homes) {
    if (!home.reports ||
        households_coming[home.owner_id] >= free_households[home.owner_id]) {
      continue;
    }
    if (queue_civilian(*world, home.id, home.owner_id)) {
      ++households_coming[home.owner_id];
    }
  }
}

} // namespace Game::Systems
