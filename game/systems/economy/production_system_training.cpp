#include "production_system_training.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "core/ambient_session.h"
#include "core/component_economy.h"
#include "core/component_gameplay.h"
#include "core/death_sequence.h"
#include "core/event_manager.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "game_config.h"
#include "map/map_transformer.h"
#include "production_service.h"
#include "systems/movement/body_profile.h"
#include "systems/movement/command_service.h"
#include "systems/nation_registry.h"
#include "systems/navigation/walkability.h"
#include "systems/owner_queries.h"
#include "systems/spawn_flare.h"
#include "systems/troop_profile_service.h"
#include "units/factory.h"
#include "units/spawn_type.h"
#include "units/unit.h"

namespace Game::Systems::ProductionTasks {

namespace {

void apply_production_profile(Engine::Core::ProductionComponent* prod,
                              Game::Systems::NationID nation_id,
                              Game::Units::TroopType troop_type) {
  if (prod == nullptr) {
    return;
  }
  const auto& profile =
      TroopProfileService::instance().get_profile_ref(nation_id, troop_type);
  prod->build_time = profile.production.build_time;
  prod->villager_cost = profile.production.cost;
}

auto resolve_nation_id(const Engine::Core::World& world,
                       int owner_id) -> Game::Systems::NationID {
  auto& registry = *Game::Session::services_for(world).nations;
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

void spawn_recruit(Engine::Core::World& world,
                   Engine::Core::Entity& building,
                   const Engine::Core::ProductionComponent& prod,
                   const Engine::Core::TransformComponent& transform,
                   const Engine::Core::UnitComponent& owner,
                   Game::Systems::NationID nation_id) {
  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    return;
  }

  float const exit_offset = 2.5F + 0.2F * float(prod.produced_count % 5);
  float const exit_angle = 0.5F * float(prod.produced_count % 8);
  QVector3D const raw_exit_pos =
      QVector3D(transform.position.x + exit_offset * std::cos(exit_angle),
                0.0F,
                transform.position.z + exit_offset * std::sin(exit_angle));

  Game::Units::SpawnParams sp;
  sp.player_id = owner.owner_id;
  sp.spawn_type = Game::Units::spawn_typeFromTroopType(prod.product_type);
  sp.ai_controlled = world.has<Engine::Core::AIControlledComponent>(building.get_id());
  sp.nation_id = nation_id;
  sp.is_initial_spawn = false;

  constexpr float k_recruit_exit_search = 50.0F;
  sp.position =
      Walkability::nearest_standable(raw_exit_pos, BodyProfile{}, k_recruit_exit_search)
          .value_or(raw_exit_pos);

  auto unit = reg->create(sp.spawn_type, world, sp);
  if (!unit) {
    return;
  }
  if (prod.rally_set) {
    CommandService::MoveOptions rally;
    rally.kind = MoveOrderKind::ScriptedMove;
    CommandService::move_unit(
        world, unit->id(), QVector3D(prod.rally_x, 0.0F, prod.rally_z), rally);
  }
  attach_spawn_flare(
      world, unit->id(), sp.spawn_type, Engine::Core::SpawnFlareStyle::Recruit);
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent::for_owner(owner.owner_id, "build.unit_ready"));
}

void start_next_queued_job(Engine::Core::ProductionComponent& prod,
                           Game::Systems::NationID nation_id) {
  if (prod.production_queue.empty()) {
    return;
  }
  prod.product_type = prod.production_queue.front();
  prod.production_queue.erase(prod.production_queue.begin());
  apply_production_profile(&prod, nation_id, prod.product_type);
  prod.time_remaining = prod.build_time;
  prod.in_progress = true;
}

void finish_training_job(Engine::Core::World& world,
                         Engine::Core::Entity& building,
                         Engine::Core::ProductionComponent& prod,
                         const Game::Systems::TroopProfile& profile,
                         Game::Systems::NationID nation_id,
                         int capacity_increment) {
  auto* t = world.try_get<Engine::Core::TransformComponent>(building.get_id());
  auto* u = world.try_get<Engine::Core::UnitComponent>(building.get_id());
  if ((t != nullptr) && (u != nullptr)) {
    int const current_troops =
        Game::Systems::authoritative_troop_count_for(world, u->owner_id);
    int const max_troops = Game::GameConfig::instance().get_max_troops_per_player();
    if (current_troops + std::max(1, profile.individuals_per_unit) > max_troops) {
      prod.in_progress = false;
      prod.time_remaining = 0.0F;
      return;
    }
    spawn_recruit(world, building, prod, *t, *u, nation_id);
    prod.produced_count += capacity_increment;
  }

  prod.in_progress = false;
  prod.time_remaining = 0.0F;
  start_next_queued_job(prod, nation_id);
}

void advance_training_job(Engine::Core::World& world,
                          Engine::Core::Entity& building,
                          Engine::Core::ProductionComponent& prod,
                          const Engine::Core::UnitComponent* unit_comp,
                          float delta_time) {
  const int owner_id = (unit_comp != nullptr) ? unit_comp->owner_id : -1;
  const auto nation_id = resolve_nation_id(world, owner_id);
  const auto& current_profile =
      TroopProfileService::instance().get_profile_ref(nation_id, prod.product_type);
  int const capacity_increment =
      production_count_increment(unit_comp, current_profile.production.cost);

  if ((unit_comp != nullptr) &&
      (unit_comp->spawn_type == Game::Units::SpawnType::Home) &&
      (prod.produced_count + capacity_increment > prod.max_units)) {
    prod.in_progress = false;
    return;
  }
  prod.time_remaining -= delta_time;
  if (prod.time_remaining <= 0.0F) {
    finish_training_job(
        world, building, prod, current_profile, nation_id, capacity_increment);
  }
}

} // namespace

void tick_completion_effects(Engine::Core::World& world, float delta_time) {
  for (auto* entity :
       world.collect_entities_with<Engine::Core::ProductionCompletionComponent>()) {
    auto* effect = entity->get_component<Engine::Core::ProductionCompletionComponent>();
    effect->remaining -= std::max(delta_time, 0.0F);
    if (effect->remaining <= 0.0F) {
      entity->remove_component<Engine::Core::ProductionCompletionComponent>();
    }
  }
}

void advance_unit_training(Engine::Core::World& world, float delta_time) {
  for (auto [entity_ref, prod_ref] :
       world.entity_view<Engine::Core::ProductionComponent>()) {
    Engine::Core::Entity* e = &entity_ref;
    auto* prod = &prod_ref;

    if (world.has<Engine::Core::DismantleSiteComponent>(e->get_id())) {
      continue;
    }

    auto* unit_comp = world.try_get<Engine::Core::UnitComponent>(e->get_id());
    if ((unit_comp != nullptr) && Game::Core::is_neutral_owner(unit_comp->owner_id)) {
      continue;
    }
    if (unit_comp != nullptr && !Engine::Core::is_live_entity(*e)) {
      continue;
    }

    prod->reserve_short = unit_comp != nullptr && ProductionService::reserve_is_short(
                                                      *prod, unit_comp->spawn_type);

    if (!prod->in_progress) {
      continue;
    }
    advance_training_job(world, *e, *prod, unit_comp, delta_time);
  }
}

} // namespace Game::Systems::ProductionTasks
