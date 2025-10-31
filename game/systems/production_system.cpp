#include "production_system.h"
#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/troop_config.h"
#include "nation_registry.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"
#include "units/unit.h"
#include <cmath>
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
  prod->buildTime = profile.production.build_time;
  prod->villagerCost = profile.individuals_per_unit;
}

auto resolve_nation_id(const Engine::Core::UnitComponent *unit,
                       int owner_id) -> Game::Systems::NationID {
  auto &registry = NationRegistry::instance();
  if (const auto *nation = registry.getNationForPlayer(owner_id)) {
    return nation->id;
  }
  return registry.default_nation_id();
}

} // namespace

void ProductionSystem::update(Engine::Core::World *world, float deltaTime) {
  if (world == nullptr) {
    return;
  }
  auto entities = world->getEntitiesWith<Engine::Core::ProductionComponent>();
  for (auto *e : entities) {
    auto *prod = e->getComponent<Engine::Core::ProductionComponent>();
    if (prod == nullptr) {
      continue;
    }

    auto *unit_comp = e->getComponent<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) &&
        Game::Core::isNeutralOwner(unit_comp->owner_id)) {
      continue;
    }

    if (!prod->inProgress) {
      continue;
    }

    const int owner_id = (unit_comp != nullptr) ? unit_comp->owner_id : -1;
    const auto nation_id = resolve_nation_id(unit_comp, owner_id);
    const auto current_profile = TroopProfileService::instance().get_profile(
        nation_id, prod->product_type);
    int const individuals_per_unit = current_profile.individuals_per_unit;

    if (prod->producedCount + individuals_per_unit > prod->maxUnits) {
      prod->inProgress = false;
      continue;
    }
    prod->timeRemaining -= deltaTime;
    if (prod->timeRemaining <= 0.0F) {

      auto *t = e->getComponent<Engine::Core::TransformComponent>();
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {

        int const current_troops =
            Engine::Core::World::countTroopsForPlayer(u->owner_id);
        int const max_troops =
            Game::GameConfig::instance().getMaxTroopsPerPlayer();
        if (current_troops + individuals_per_unit > max_troops) {
          prod->inProgress = false;
          prod->timeRemaining = 0.0F;
          continue;
        }

        float const exit_offset = 2.5F + 0.2F * float(prod->producedCount % 5);
        float const exit_angle = 0.5F * float(prod->producedCount % 8);
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
          sp.aiControlled =
              e->hasComponent<Engine::Core::AIControlledComponent>();
          sp.nation_id = nation_id;
          auto unit = reg->create(sp.spawn_type, *world, sp);

          if (unit && prod->rallySet) {
            unit->moveTo(prod->rallyX, prod->rallyZ);
          }
        }

        prod->producedCount += individuals_per_unit;
      }

      prod->inProgress = false;
      prod->timeRemaining = 0.0F;

      if (!prod->productionQueue.empty()) {
        prod->product_type = prod->productionQueue.front();
        prod->productionQueue.erase(prod->productionQueue.begin());
        apply_production_profile(prod, nation_id, prod->product_type);
        prod->timeRemaining = prod->buildTime;
        prod->inProgress = true;
      }
    }
  }
}

} // namespace Game::Systems
