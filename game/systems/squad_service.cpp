#include "squad_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

#include "../core/world.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/squad.h"
#include "../units/unit.h"
#include "nav_grid.h"

namespace Game::Systems {

namespace {

auto unit_of(Engine::Core::World& world,
             Engine::Core::EntityID id) -> Engine::Core::UnitComponent* {
  return world.try_get<Engine::Core::UnitComponent>(id);
}

auto unit_of(const Engine::Core::World& world,
             Engine::Core::EntityID id) -> const Engine::Core::UnitComponent* {
  return world.try_get<Engine::Core::UnitComponent>(id);
}

auto transform_of(Engine::Core::World& world,
                  Engine::Core::EntityID id) -> Engine::Core::TransformComponent* {
  return world.try_get<Engine::Core::TransformComponent>(id);
}

auto full_max_health(const Engine::Core::UnitComponent& unit) -> int {
  const float fraction = std::max(0.01F, Game::Units::squad_fraction(unit));
  return std::max(
      1, static_cast<int>(std::lround(static_cast<float>(unit.max_health) / fraction)));
}

void scale_health_pool(Engine::Core::UnitComponent& unit,
                       int establishment_max_health,
                       int strength,
                       float health_ratio) {
  const int establishment = Game::Units::squad_establishment(unit.spawn_type);
  const int clamped = std::clamp(strength, 1, establishment);
  unit.squad_strength = clamped >= establishment ? 0 : clamped;
  const float fraction =
      static_cast<float>(clamped) / static_cast<float>(establishment);
  unit.max_health =
      std::max(1,
               static_cast<int>(std::lround(
                   static_cast<float>(establishment_max_health) * fraction)));
  unit.health = std::clamp(
      static_cast<int>(std::lround(static_cast<float>(unit.max_health) * health_ratio)),
      1,
      unit.max_health);
}

auto detachment_position(Engine::Core::World& world,
                         const Engine::Core::TransformComponent& parent) -> QVector3D {
  (void)world;
  constexpr float k_step = 2.5F;
  const Point origin =
      NavGrid::world_to_grid(parent.position.x + k_step, parent.position.z + k_step);
  if (const auto cell = NavGrid::find_nearest_walkable_grid(origin, 6)) {
    const QVector3D placed = NavGrid::grid_to_world(*cell);
    return {placed.x(), parent.position.y, placed.z()};
  }
  return {parent.position.x + k_step, parent.position.y, parent.position.z + k_step};
}

} // namespace

auto SquadService::can_divide(const Engine::Core::World& world,
                              Engine::Core::EntityID unit_id) -> bool {
  const auto* unit = unit_of(world, unit_id);
  return unit != nullptr && Game::Units::squad_can_divide(*unit);
}

void SquadService::apply_strength(Engine::Core::World& world,
                                  Engine::Core::EntityID unit_id,
                                  int strength) {
  auto* unit = unit_of(world, unit_id);
  if (unit == nullptr || unit->max_health <= 0) {
    return;
  }
  const float ratio = std::clamp(static_cast<float>(unit->health) /
                                     static_cast<float>(unit->max_health),
                                 0.01F,
                                 1.0F);
  scale_health_pool(*unit, full_max_health(*unit), strength, ratio);
}

auto SquadService::divide(Engine::Core::World& world,
                          Engine::Core::EntityID unit_id) -> SquadDivision {
  SquadDivision result;
  auto* unit = unit_of(world, unit_id);
  auto* transform = transform_of(world, unit_id);
  if (unit == nullptr || transform == nullptr ||
      !Game::Units::squad_can_divide(*unit)) {
    return result;
  }

  auto registry = Game::Map::MapTransformer::get_factory_registry();
  if (!registry) {
    return result;
  }

  const int strength = Game::Units::squad_strength(*unit);
  const int kept = strength / 2;
  const int detached = strength - kept;
  const int establishment_max_health = full_max_health(*unit);
  const float ratio = std::clamp(static_cast<float>(unit->health) /
                                     static_cast<float>(std::max(1, unit->max_health)),
                                 0.01F,
                                 1.0F);

  Game::Units::SpawnParams params;
  params.position = detachment_position(world, *transform);
  params.rotation_y = transform->rotation.y;
  params.player_id = unit->owner_id;
  params.spawn_type = unit->spawn_type;
  params.ai_controlled =
      world.try_get<Engine::Core::AIControlledComponent>(unit_id) != nullptr;
  params.nation_id = unit->nation_id;
  params.is_initial_spawn = false;

  auto spawned = registry->create(unit->spawn_type, world, params);
  if (!spawned) {
    return result;
  }

  result.parent = unit_id;
  result.detachment = spawned->id();

  auto* parent_unit = unit_of(world, unit_id);
  auto* child_unit = unit_of(world, result.detachment);
  if (parent_unit == nullptr || child_unit == nullptr) {
    return result;
  }

  scale_health_pool(*parent_unit, establishment_max_health, kept, ratio);
  scale_health_pool(*child_unit, establishment_max_health, detached, ratio);

  return result;
}

auto SquadService::can_merge(const Engine::Core::World& world,
                             Engine::Core::EntityID kept,
                             Engine::Core::EntityID absorbed) -> bool {
  if (kept == absorbed || kept == 0 || absorbed == 0) {
    return false;
  }
  const auto* left = unit_of(world, kept);
  const auto* right = unit_of(world, absorbed);
  if (left == nullptr || right == nullptr) {
    return false;
  }
  if (left->owner_id != right->owner_id || left->spawn_type != right->spawn_type) {
    return false;
  }
  if (left->health <= 0 || right->health <= 0) {
    return false;
  }
  if (Game::Units::is_building_spawn(left->spawn_type)) {
    return false;
  }
  if (Game::Units::squad_is_at_full_strength(*left)) {
    return false;
  }

  const auto* left_transform = world.try_get<Engine::Core::TransformComponent>(kept);
  const auto* right_transform =
      world.try_get<Engine::Core::TransformComponent>(absorbed);
  if (left_transform == nullptr || right_transform == nullptr) {
    return false;
  }
  const float dx = left_transform->position.x - right_transform->position.x;
  const float dz = left_transform->position.z - right_transform->position.z;
  return (dx * dx) + (dz * dz) <= k_merge_radius * k_merge_radius;
}

auto SquadService::merge(Engine::Core::World& world,
                         Engine::Core::EntityID kept,
                         Engine::Core::EntityID absorbed) -> bool {
  if (!can_merge(world, kept, absorbed)) {
    return false;
  }
  auto* left = unit_of(world, kept);
  auto* right = unit_of(world, absorbed);

  const int establishment = Game::Units::squad_establishment(left->spawn_type);
  const int establishment_max_health = full_max_health(*left);
  const int combined_strength = std::min(establishment,
                                         Game::Units::squad_strength(*left) +
                                             Game::Units::squad_strength(*right));
  const int combined_health = left->health + right->health;

  const int combined_max_health = std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(establishment_max_health) *
                                   static_cast<float>(combined_strength) /
                                   static_cast<float>(establishment))));
  const float ratio = std::clamp(static_cast<float>(combined_health) /
                                     static_cast<float>(combined_max_health),
                                 0.01F,
                                 1.0F);

  scale_health_pool(*left, establishment_max_health, combined_strength, ratio);
  world.destroy_entity(absorbed);

  return true;
}

auto SquadService::divide_all(Engine::Core::World& world,
                              const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<SquadDivision> {
  std::vector<SquadDivision> divisions;
  divisions.reserve(units.size());
  for (const auto id : units) {
    auto division = divide(world, id);
    if (division.detachment != 0) {
      divisions.push_back(division);
    }
  }
  return divisions;
}

auto SquadService::merge_all(Engine::Core::World& world,
                             const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<SquadMerge> {
  std::vector<SquadMerge> merges;

  std::vector<Engine::Core::EntityID> remaining = units;
  std::sort(remaining.begin(), remaining.end());
  remaining.erase(std::unique(remaining.begin(), remaining.end()), remaining.end());

  bool merged_this_round = true;
  while (merged_this_round) {
    merged_this_round = false;
    for (std::size_t i = 0; i < remaining.size() && !merged_this_round; ++i) {
      for (std::size_t j = i + 1; j < remaining.size(); ++j) {
        if (!merge(world, remaining[i], remaining[j])) {
          continue;
        }
        merges.push_back(SquadMerge{remaining[i], remaining[j]});
        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(j));
        merged_this_round = true;
        break;
      }
    }
  }
  return merges;
}

} // namespace Game::Systems
