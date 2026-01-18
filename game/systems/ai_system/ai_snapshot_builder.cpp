#include "ai_snapshot_builder.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "systems/ai_system/ai_types.h"
#include <utility>

namespace Game::Systems::AI {

auto AISnapshotBuilder::build(const Engine::Core::World &world,
                              int aiOwnerId) -> AISnapshot {
  AISnapshot snapshot;
  snapshot.player_id = aiOwnerId;

  auto friendlies = world.get_units_owned_by(aiOwnerId);
  snapshot.friendly_units.reserve(friendlies.size());

  int skipped_no_ai = 0;
  int skipped_no_unit = 0;
  int skipped_dead = 0;
  int added = 0;

  for (auto *entity : friendlies) {
    if (!entity->has_component<Engine::Core::AIControlledComponent>()) {
      skipped_no_ai++;
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      skipped_no_unit++;
      continue;
    }

    if (unit->health <= 0) {
      skipped_dead++;
      continue;
    }

    EntitySnapshot data;
    data.id = entity->get_id();
    data.spawn_type = unit->spawn_type;
    data.owner_id = unit->owner_id;
    data.health = unit->health;
    data.max_health = unit->max_health;
    data.is_building = entity->has_component<Engine::Core::BuildingComponent>();

    if (auto *transform =
            entity->get_component<Engine::Core::TransformComponent>()) {
      data.pos_x = transform->position.x;
      data.pos_y = 0.0F;
      data.pos_z = transform->position.z;
    }

    if (auto *movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      data.movement.has_component = true;
      data.movement.has_target = movement->has_target;
    }

    if (auto *production =
            entity->get_component<Engine::Core::ProductionComponent>()) {
      data.production.has_component = true;
      data.production.in_progress = production->in_progress;
      data.production.build_time = production->build_time;
      data.production.time_remaining = production->time_remaining;
      data.production.produced_count = production->produced_count;
      data.production.max_units = production->max_units;
      data.production.product_type = production->product_type;
      data.production.rally_set = production->rally_set;
      data.production.rally_x = production->rally_x;
      data.production.rally_z = production->rally_z;
      data.production.queue_size =
          static_cast<int>(production->production_queue.size());
    }

    if (auto *builder_prod =
            entity->get_component<Engine::Core::BuilderProductionComponent>()) {
      data.builder_production.has_component = true;
      data.builder_production.has_construction_site =
          builder_prod->has_construction_site;
      data.builder_production.in_progress = builder_prod->in_progress;
      data.builder_production.at_construction_site =
          builder_prod->at_construction_site;
    }

    snapshot.friendly_units.push_back(std::move(data));
    added++;
  }

  auto enemies = world.get_enemy_units(aiOwnerId);
  snapshot.visible_enemies.reserve(enemies.size());

  for (auto *entity : enemies) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    ContactSnapshot contact;
    contact.id = entity->get_id();
    contact.owner_id = unit->owner_id;
    contact.is_building =
        entity->has_component<Engine::Core::BuildingComponent>();
    contact.pos_x = transform->position.x;
    contact.pos_y = 0.0F;
    contact.pos_z = transform->position.z;

    contact.health = unit->health;
    contact.max_health = unit->max_health;
    contact.spawn_type = unit->spawn_type;

    snapshot.visible_enemies.push_back(std::move(contact));
  }

  return snapshot;
}

} // namespace Game::Systems::AI
