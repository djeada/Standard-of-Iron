#include "ai_snapshot_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../core/component_economy.h"
#include "../../core/world.h"
#include "../../game_config.h"
#include "../../map/terrain_service.h"
#include "../../session/session_context.h"
#include "../../units/squad.h"
#include "../combat_system/target_rules.h"
#include "../nation_registry.h"
#include "../owner_queries.h"
#include "../player_resource_registry.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace {

struct VisionSource {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float radius_sq = 0.0F;
};

constexpr float k_min_building_vision_range = 18.0F;

class GarrisonOwners {
public:
  explicit GarrisonOwners(const Game::Systems::NationRegistry& nations)
      : m_nations(&nations) {}

  [[nodiscard]] auto holds_ground(int owner_id) -> bool {
    auto known = m_resolved.find(owner_id);
    if (known != m_resolved.end()) {
      return known->second;
    }
    const auto* nation = m_nations->get_nation_for_player(owner_id);
    const bool garrison = nation != nullptr && !nation->has_economy;
    m_resolved.emplace(owner_id, garrison);
    return garrison;
  }

private:
  const Game::Systems::NationRegistry* m_nations;
  std::unordered_map<int, bool> m_resolved;
};

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

class PointGrid {
public:
  explicit PointGrid(float cell_size)
      : m_cell(std::max(cell_size, 1.0F)) {}

  void insert(float x, float z, std::size_t index) {
    m_cells[pack(cell_of(x), cell_of(z))].push_back(index);
  }

  template <typename Fn>
  auto any_near(float x, float z, Fn&& fn) const -> bool {
    const int center_x = cell_of(x);
    const int center_z = cell_of(z);
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dx = -1; dx <= 1; ++dx) {
        const auto found = m_cells.find(pack(center_x + dx, center_z + dz));
        if (found == m_cells.end()) {
          continue;
        }
        for (const std::size_t index : found->second) {
          if (fn(index)) {
            return true;
          }
        }
      }
    }
    return false;
  }

private:
  [[nodiscard]] auto cell_of(float value) const -> int {
    return static_cast<int>(std::floor(value / m_cell));
  }
  static auto pack(int x, int z) -> std::int64_t {
    return (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z);
  }

  float m_cell;
  std::unordered_map<std::int64_t, std::vector<std::size_t>> m_cells;
};

auto build_vision_grid(const std::vector<VisionSource>& sources) -> PointGrid {
  float widest = 1.0F;
  for (const auto& source : sources) {
    widest = std::max(widest, std::sqrt(source.radius_sq));
  }
  PointGrid grid(widest);
  for (std::size_t index = 0; index < sources.size(); ++index) {
    grid.insert(sources[index].x, sources[index].z, index);
  }
  return grid;
}

auto is_visible_to_sources(const Engine::Core::TransformComponent& transform,
                           const std::vector<VisionSource>& sources,
                           const PointGrid& grid) -> bool {
  const float x = transform.position.x;
  const float z = transform.position.z;
  return grid.any_near(x, z, [&](std::size_t index) {
    const VisionSource& source = sources[index];
    const float dx = x - source.x;
    const float dz = z - source.z;
    return (dx * dx + dz * dz) <= source.radius_sq;
  });
}

} // namespace

namespace Game::Systems::AI {

void AISnapshotBuilder::attach_nation(AISnapshot& snapshot,
                                      int ai_owner_id,
                                      const Game::Systems::NationRegistry& nations) {
  const auto* nation = nations.get_nation_for_player(ai_owner_id);
  if (nation != nullptr) {
    snapshot.nation = std::make_shared<const Game::Systems::Nation>(*nation);
  }
}

auto AISnapshotBuilder::build(const Engine::Core::World& world,
                              int ai_owner_id) -> AISnapshot {
  auto& session = Game::Session::session_for(world);
  AISnapshot snapshot;
  snapshot.player_id = ai_owner_id;
  snapshot.resources = session.economy().get_all(ai_owner_id);
  snapshot.has_resource_snapshot = true;

  auto& terrain_service = session.terrain();
  for (const auto& prop : terrain_service.world_props()) {
    if (!Game::Map::is_harvestable_world_prop_type(prop.type)) {
      continue;
    }
    const QVector3D position = terrain_service.world_prop_world_position(prop);
    snapshot.resource_nodes.push_back(
        {prop.id,
         prop.type,
         position.x(),
         position.z(),
         terrain_service.is_world_prop_reserved(prop.id)});
  }

  auto friendlies = world.get_units_owned_by(ai_owner_id);
  const auto* nation = session.nations().get_nation_for_player(ai_owner_id);
  GarrisonOwners garrison_owners(session.nations());
  attach_nation(snapshot, ai_owner_id, session.nations());
  snapshot.max_troops_per_player =
      Game::GameConfig::instance().get_max_troops_per_player();
  if (const auto* height_map =
          terrain_service.is_initialized() ? terrain_service.get_height_map() : nullptr;
      height_map != nullptr && height_map->get_width() > 0 &&
      height_map->get_height() > 0) {
    const float tile = height_map->get_tile_size();
    const float half_w = static_cast<float>(height_map->get_width()) * 0.5F - 0.5F;
    const float half_h = static_cast<float>(height_map->get_height()) * 0.5F - 0.5F;
    snapshot.has_map_bounds = true;
    snapshot.map_min_x = -half_w * tile;
    snapshot.map_max_x = half_w * tile;
    snapshot.map_min_z = -half_h * tile;
    snapshot.map_max_z = half_h * tile;
  }
  const auto vision_sources = collect_vision_sources(friendlies);
  const PointGrid vision_grid = build_vision_grid(vision_sources);
  snapshot.friendly_units.reserve(friendlies.size());

  for (auto* entity : friendlies) {
    if (!world.has<Engine::Core::AIControlledComponent>(entity->get_id())) {
      continue;
    }

    const auto* guard_mode =
        world.try_get<Engine::Core::GuardModeComponent>(entity->get_id());
    if ((guard_mode != nullptr) && guard_mode->active && guard_mode->has_guard_target) {
      continue;
    }

    const auto* patrol = world.try_get<Engine::Core::PatrolComponent>(entity->get_id());
    if ((patrol != nullptr) && patrol->patrolling) {
      continue;
    }

    const auto* hold_mode =
        world.try_get<Engine::Core::HoldModeComponent>(entity->get_id());
    if ((hold_mode != nullptr) && hold_mode->active) {
      continue;
    }

    auto* unit = world.try_get<Engine::Core::UnitComponent>(entity->get_id());
    if (unit == nullptr) {
      continue;
    }

    const auto* assault_wave =
        world.try_get<Engine::Core::AssaultWaveComponent>(entity->get_id());
    const bool is_assault = (assault_wave != nullptr) && assault_wave->active;

    if (unit->health <= 0) {
      continue;
    }

    EntitySnapshot data;
    data.id = entity->get_id();
    data.spawn_type = unit->spawn_type;
    data.owner_id = unit->owner_id;
    data.health = unit->health;
    data.max_health = unit->max_health;
    data.is_building = world.has<Engine::Core::BuildingComponent>(entity->get_id());
    data.is_commander = world.has<Engine::Core::CommanderComponent>(entity->get_id());
    data.squad_strength = Game::Units::squad_strength(*unit);
    data.squad_establishment = Game::Units::squad_establishment(unit->spawn_type);
    data.is_assault = is_assault;
    data.has_delivery_order =
        world.has<Engine::Core::CivilianDeliveryComponent>(data.id);
    if (const auto* crop = world.try_get<Engine::Core::FarmComponent>(data.id);
        crop != nullptr) {
      data.crop_is_ripe =
          crop->ripe() && !world.has<Engine::Core::DismantleSiteComponent>(data.id);
    }
    if (is_assault) {
      data.has_march_target = assault_wave->has_march_target;
      data.march_target_x = assault_wave->march_target_x;
      data.march_target_z = assault_wave->march_target_z;
    }

    if (auto* transform =
            world.try_get<Engine::Core::TransformComponent>(entity->get_id())) {
      data.pos_x = transform->position.x;
      data.pos_y = 0.0F;
      data.pos_z = transform->position.z;
    }

    if (auto* movement =
            world.try_get<Engine::Core::MovementComponent>(entity->get_id())) {
      data.movement.has_component = true;
      data.movement.has_target = movement->get_has_target();
      data.movement.has_objective = movement->get_has_requested_goal();
      data.movement.objective_x = movement->get_requested_goal_x();
      data.movement.objective_z = movement->get_requested_goal_z();
    }

    if (auto* facts =
            world.try_get<Engine::Core::MovementFactsComponent>(entity->get_id())) {
      const auto& stall = facts->progress.stall;
      data.movement.stalled_seconds = stall.stalled_seconds;
      data.movement.stalled = stall.rung != Engine::Core::MovementRecoveryRung::None &&
                              data.movement.has_target;
      data.movement.objective_abandoned = stall.objective_abandoned;
      data.movement.abandon_count = static_cast<int>(stall.abandon_count);
    }

    if (auto* production =
            world.try_get<Engine::Core::ProductionComponent>(entity->get_id())) {
      data.production.has_component = true;
      data.production.in_progress = production->in_progress;
      data.production.build_time = production->build_time;
      data.production.time_remaining = production->time_remaining;
      data.production.produced_count = production->produced_count;
      data.production.max_units = production->max_units;
      data.production.manpower_available = production->manpower_available;
      data.production.product_type = production->product_type;
      data.production.rally_set = production->rally_set;
      data.production.rally_x = production->rally_x;
      data.production.rally_z = production->rally_z;
      data.production.queue_size =
          static_cast<int>(production->production_queue.size());
    }

    if (auto* builder_prod =
            world.try_get<Engine::Core::BuilderProductionComponent>(entity->get_id())) {
      data.builder_production.has_component = true;
      data.builder_production.has_construction_site =
          builder_prod->has_construction_site;
      data.builder_production.in_progress = builder_prod->in_progress;
      data.builder_production.at_construction_site = builder_prod->at_construction_site;
      data.builder_production.has_task_target =
          builder_prod->has_task_target || builder_prod->structure_task_entity_id != 0;

      if (builder_prod->structure_task_entity_id != 0) {
        data.builder_production.task_target_id = builder_prod->structure_task_entity_id;
      } else if (builder_prod->has_task_target) {
        data.builder_production.task_target_id = builder_prod->task_target_id;
      }
      if (const auto* carry =
              world.try_get<Engine::Core::ResourceCarryComponent>(data.id);
          carry != nullptr && !carry->empty()) {
        data.builder_production.carrying_load = true;
      }
      data.builder_production.auto_gather = builder_prod->auto_gather;
      data.builder_production.construction_site_x = builder_prod->construction_site_x;
      data.builder_production.construction_site_z = builder_prod->construction_site_z;

      if (builder_prod->has_construction_site) {
        if (const auto raising =
                Game::Units::spawn_typeFromString(builder_prod->product_type);
            raising.has_value() && Game::Units::is_building_spawn(*raising)) {
          data.builder_production.raising_a_building = true;
          data.builder_production.building_under_way = *raising;
        }
      }
    }

    snapshot.friendly_units.push_back(std::move(data));
  }

  auto enemies = Game::Systems::Combat::collect_hostile_contacts(world, ai_owner_id);
  snapshot.visible_enemies.reserve(enemies.size());
  snapshot.strategic_objectives.reserve(enemies.size());

  if (nation != nullptr && !nation->has_economy) {
    const auto& world_props = terrain_service.world_props();
    snapshot.defense_anchors.reserve(world_props.size());
    for (const auto& prop : world_props) {
      if (prop.type != Game::Map::WorldProp::Type::Ruins &&
          prop.type != Game::Map::WorldProp::Type::MagicShrine) {
        continue;
      }

      ContactSnapshot anchor;
      anchor.id = static_cast<Engine::Core::EntityID>(prop.id);
      anchor.pos_x = prop.x;
      anchor.pos_y = 0.0F;
      anchor.pos_z = prop.z;
      anchor.health = 1;
      anchor.max_health = 1;
      snapshot.defense_anchors.push_back(std::move(anchor));
    }
  }

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
      objective.holds_ground = garrison_owners.holds_ground(unit->owner_id);
      snapshot.strategic_objectives.push_back(std::move(objective));
    }

    if (!is_visible_to_sources(*transform, vision_sources, vision_grid)) {
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
    contact.holds_ground = garrison_owners.holds_ground(unit->owner_id);

    snapshot.visible_enemies.push_back(std::move(contact));
  }

  const float engaged_radius_sq =
      Game::Systems::AI::k_engaged_radius * Game::Systems::AI::k_engaged_radius;
  PointGrid engagement_grid(Game::Systems::AI::k_engaged_radius);
  for (std::size_t index = 0; index < snapshot.visible_enemies.size(); ++index) {
    const auto& enemy = snapshot.visible_enemies[index];
    if (enemy.is_building) {
      continue;
    }
    engagement_grid.insert(enemy.pos_x, enemy.pos_z, index);
  }
  for (auto& friendly : snapshot.friendly_units) {
    friendly.engagement_resolved = true;
    friendly.engaged = engagement_grid.any_near(
        friendly.pos_x, friendly.pos_z, [&](std::size_t index) {
          const auto& enemy = snapshot.visible_enemies[index];
          const float dx = enemy.pos_x - friendly.pos_x;
          const float dy = enemy.pos_y - friendly.pos_y;
          const float dz = enemy.pos_z - friendly.pos_z;
          return (dx * dx + dy * dy + dz * dz) <= engaged_radius_sq;
        });
  }

  return snapshot;
}

} // namespace Game::Systems::AI
