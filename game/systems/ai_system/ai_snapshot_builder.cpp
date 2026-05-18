#include "ai_snapshot_builder.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "systems/ai_system/ai_types.h"

namespace {

struct VisionSource {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float radius_sq = 0.0F;
};

constexpr float k_min_building_vision_range = 18.0F;

auto collect_vision_sources(const std::vector<Engine::Core::Entity*>& entities)
    -> std::vector<VisionSource> {
  std::vector<VisionSource> sources;
  sources.reserve(entities.size());

  for (auto* entity : entities) {
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if ((unit == nullptr) || (transform == nullptr) || unit->health <= 0) {
      continue;
    }

    float vision_range = unit->vision_range;
    if (entity->has_component<Engine::Core::BuildingComponent>()) {
      vision_range = std::max(vision_range, k_min_building_vision_range);
    }

    if (vision_range <= 0.0F) {
      continue;
    }

    sources.push_back({transform->position.x,
                       0.0F,
                       transform->position.z,
                       vision_range * vision_range});
  }

  return sources;
}

auto is_visible_to_sources(const Engine::Core::TransformComponent& transform,
                           const std::vector<VisionSource>& sources) -> bool {
  for (const auto& source : sources) {
    const float dx = transform.position.x - source.x;
    const float dz = transform.position.z - source.z;
    const float dist_sq = dx * dx + dz * dz;
    if (dist_sq <= source.radius_sq) {
      return true;
    }
  }

  return false;
}

} // namespace

namespace Game::Systems::AI {

auto AISnapshotBuilder::build(const Engine::Core::World& world,
                              int ai_owner_id) -> AISnapshot {
  AISnapshot snapshot;
  snapshot.player_id = ai_owner_id;

  auto friendlies = world.get_units_owned_by(ai_owner_id);
  const auto vision_sources = collect_vision_sources(friendlies);
  snapshot.friendly_units.reserve(friendlies.size());

  for (auto* entity : friendlies) {
    if (!entity->has_component<Engine::Core::AIControlledComponent>()) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    EntitySnapshot data;
    data.id = entity->get_id();
    data.spawn_type = unit->spawn_type;
    data.owner_id = unit->owner_id;
    data.health = unit->health;
    data.max_health = unit->max_health;
    data.is_building = entity->has_component<Engine::Core::BuildingComponent>();
    data.is_commander = entity->has_component<Engine::Core::CommanderComponent>();

    if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
      data.pos_x = transform->position.x;
      data.pos_y = 0.0F;
      data.pos_z = transform->position.z;
    }

    if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
      data.movement.has_component = true;
      data.movement.has_target = movement->has_target;
    }

    if (auto* production = entity->get_component<Engine::Core::ProductionComponent>()) {
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

    if (auto* builder_prod =
            entity->get_component<Engine::Core::BuilderProductionComponent>()) {
      data.builder_production.has_component = true;
      data.builder_production.has_construction_site =
          builder_prod->has_construction_site;
      data.builder_production.in_progress = builder_prod->in_progress;
      data.builder_production.at_construction_site = builder_prod->at_construction_site;
      data.builder_production.construction_site_x = builder_prod->construction_site_x;
      data.builder_production.construction_site_z = builder_prod->construction_site_z;
    }

    snapshot.friendly_units.push_back(std::move(data));
  }

  auto enemies = world.get_enemy_units(ai_owner_id);
  snapshot.visible_enemies.reserve(enemies.size());
  snapshot.strategic_objectives.reserve(enemies.size());

  for (auto* entity : enemies) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    const bool is_building = entity->has_component<Engine::Core::BuildingComponent>();
    const bool is_commander = entity->has_component<Engine::Core::CommanderComponent>();

    if (is_building || is_commander) {
      ContactSnapshot objective;
      objective.id = entity->get_id();
      objective.owner_id = unit->owner_id;
      objective.is_building = is_building;
      objective.pos_x = transform->position.x;
      objective.pos_y = 0.0F;
      objective.pos_z = transform->position.z;
      objective.health = unit->health;
      objective.max_health = unit->max_health;
      objective.spawn_type = unit->spawn_type;
      snapshot.strategic_objectives.push_back(std::move(objective));
    }

    if (!is_visible_to_sources(*transform, vision_sources)) {
      continue;
    }

    ContactSnapshot contact;
    contact.id = entity->get_id();
    contact.owner_id = unit->owner_id;
    contact.is_building = is_building;
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
