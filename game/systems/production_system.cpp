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

  auto builder_entities =
      world->get_entities_with<Engine::Core::BuilderProductionComponent>();
  for (auto *e : builder_entities) {
    auto *builder_prod =
        e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod == nullptr || !builder_prod->in_progress) {
      continue;
    }

    builder_prod->time_remaining -= delta_time;
    if (builder_prod->time_remaining <= 0.0F) {

      auto *t = e->get_component<Engine::Core::TransformComponent>();
      auto *u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {
        auto reg = Game::Map::MapTransformer::getFactoryRegistry();
        if (reg) {
          Game::Units::SpawnParams sp;

          sp.position = QVector3D(t->position.x, t->position.y, t->position.z);
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
          } else {

            builder_prod->in_progress = false;
            builder_prod->time_remaining = 0.0F;
            continue;
          }

          reg->create(sp.spawn_type, *world, sp);
        }
      }

      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->construction_complete = true;
    }
  }
}

} // namespace Game::Systems
