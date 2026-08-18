#include "app/mission/tutorial_observation.h"

#include <QLatin1String>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "app/viewmodels/placement_view_model.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"

namespace App::Mission {

auto observe_tutorial_frame(const TutorialObservationInputs& inputs)
    -> Game::Mission::TutorialObservation {
  const auto& notes = inputs.notes;
  Game::Mission::TutorialObservation o;
  o.mission_running = inputs.world != nullptr && inputs.mission_running;
  o.victory = inputs.victory_state == QLatin1String("victory");
  o.defeat = inputs.victory_state == QLatin1String("defeat");
  if (!o.mission_running) {
    return o;
  }

  o.move_order_accepted = notes.move_accepted;
  o.attack_order_accepted = notes.attack_accepted;
  o.hold_order_accepted = notes.hold_accepted;
  o.guard_order_accepted = notes.guard_accepted;
  o.patrol_order_accepted = notes.patrol_accepted;
  o.gather_order_accepted = notes.gather_accepted;
  o.build_order_accepted = notes.build_accepted;
  o.last_rejection_reason = notes.last_rejection_reason;
  o.speed_changed = notes.speed_changed;
  o.camera_used = notes.camera_used;

  const int owner = inputs.local_owner_id;
  o.enemy_troops_defeated = inputs.enemy_troops_defeated;

  const auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  const auto harvested = resources.get_harvested_all(owner);
  const auto stock = resources.get_all(owner);
  o.harvested_wood = harvested.get(Game::Systems::ResourceType::Wood);
  o.harvested_stone = harvested.get(Game::Systems::ResourceType::Stone);
  o.harvested_iron = harvested.get(Game::Systems::ResourceType::Iron);
  o.wood = stock.get(Game::Systems::ResourceType::Wood);
  o.stone = stock.get(Game::Systems::ResourceType::Stone);
  o.iron = stock.get(Game::Systems::ResourceType::Iron);

  if (auto* selection = inputs.world->get_system<Game::Systems::SelectionSystem>()) {
    for (const auto id : selection->get_selected_units()) {
      const auto* entity = inputs.world->get_entity(id);
      const auto* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (unit == nullptr || unit->owner_id != owner || unit->health <= 0) {
        continue;
      }
      if (Game::Units::is_building_spawn(unit->spawn_type)) {
        ++o.selected_building_count;
        if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
          ++o.selected_barracks_count;
        }
        continue;
      }
      if (unit->spawn_type == Game::Units::SpawnType::Builder) {
        ++o.selected_builder_count;
        continue;
      }
      if (entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
        o.commander_selected = true;
      }
      if (unit->spawn_type != Game::Units::SpawnType::Civilian) {
        ++o.selected_troop_count;
      }
    }
  }

  const auto& owners = Game::Systems::OwnerRegistry::instance();
  inputs.world->for_each_entity([&](const Engine::Core::Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      return;
    }
    const auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
    if (unit->owner_id != owner) {
      if (commander != nullptr && owners.are_enemies(owner, unit->owner_id)) {
        ++o.enemy_commanders_alive;
      }
      return;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Home &&
        entity.get_component<Engine::Core::WallConstructionSiteComponent>() ==
            nullptr) {
      ++o.home_count;
      return;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      if (const auto* production =
              entity.get_component<Engine::Core::ProductionComponent>()) {
        o.barracks_manpower += production->manpower_available;
        o.production_in_progress = o.production_in_progress || production->in_progress;
      }
      return;
    }
    if (commander != nullptr) {
      o.aura_ready = o.aura_ready || commander->can_activate_aura_ability();
      o.aura_active = o.aura_active || commander->aura_ability_active ||
                      commander->aura_ability_requested;
      return;
    }
    if (Game::Units::is_troop_spawn(unit->spawn_type) &&
        unit->spawn_type != Game::Units::SpawnType::Builder &&
        unit->spawn_type != Game::Units::SpawnType::Civilian) {
      ++o.soldier_count;
    }
  });

  if (inputs.placement != nullptr) {
    o.construction_preview_active = inputs.placement->construction_preview_active();
    o.construction_preview_valid = inputs.placement->construction_preview_valid();
  }

  const QVariantMap& waves = inputs.wave_status;
  o.waves_cleared = waves.value(QStringLiteral("cleared")).toInt();
  o.wave_live = waves.value(QStringLiteral("live_enemies")).toInt() > 0;
  o.wave_pending = waves.value(QStringLiteral("active")).toBool() &&
                   waves.value(QStringLiteral("seconds_until_next")).toDouble() >= 0.0;
  return o;
}

namespace {

constexpr int k_max_focus_points = 6;
constexpr float k_scout_search_radius = 38.0F;

struct FocusCandidate {
  QVector3D position;
  float distance_sq = 0.0F;
};

auto to_point(const QVector3D& position) -> QVariantMap {
  QVariantMap point;
  point[QStringLiteral("world_x")] = position.x();
  point[QStringLiteral("world_z")] = position.z();
  return point;
}

void keep_nearest(std::vector<FocusCandidate>& candidates, std::size_t limit) {
  if (candidates.size() <= limit) {
    return;
  }
  std::partial_sort(candidates.begin(),
                    candidates.begin() + static_cast<std::ptrdiff_t>(limit),
                    candidates.end(),
                    [](const FocusCandidate& lhs, const FocusCandidate& rhs) {
                      return lhs.distance_sq < rhs.distance_sq;
                    });
  candidates.resize(limit);
}

auto nearest_props(const QVector3D& anchor,
                   bool (*matches)(Game::Map::WorldProp::Type),
                   int limit) -> std::vector<QVector3D> {
  std::vector<FocusCandidate> candidates;
  for (const auto& prop : Game::Map::TerrainService::instance().world_props()) {
    if (!matches(prop.type)) {
      continue;
    }
    const float dx = prop.x - anchor.x();
    const float dz = prop.z - anchor.z();
    candidates.push_back({QVector3D(prop.x, 0.0F, prop.z), dx * dx + dz * dz});
  }
  keep_nearest(candidates, static_cast<std::size_t>(std::max(0, limit)));
  std::sort(candidates.begin(),
            candidates.end(),
            [](const FocusCandidate& lhs, const FocusCandidate& rhs) {
              return lhs.distance_sq < rhs.distance_sq;
            });
  std::vector<QVector3D> positions;
  positions.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    positions.push_back(candidate.position);
  }
  return positions;
}

auto is_tree(Game::Map::WorldProp::Type type) -> bool {
  return Game::Map::is_tree_world_prop_type(type);
}

auto is_boulder(Game::Map::WorldProp::Type type) -> bool {
  return Game::Map::is_boulder_world_prop_type(type);
}

auto is_iron_ore(Game::Map::WorldProp::Type type) -> bool {
  return Game::Map::is_iron_ore_world_prop_type(type);
}

struct OwnedUnits {
  std::vector<QVector3D> troops;
  std::vector<QVector3D> builders;
  std::vector<QVector3D> barracks;
  std::vector<QVector3D> commander;
  std::vector<QVector3D> enemy_troops;
  std::vector<QVector3D> enemy_camp;
  QVector3D anchor;
  bool has_anchor = false;
};

auto collect_units(Engine::Core::World* world, int owner) -> OwnedUnits {
  OwnedUnits units;
  const auto& owners = Game::Systems::OwnerRegistry::instance();
  world->for_each_entity([&](const Engine::Core::Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->health <= 0) {
      return;
    }
    const QVector3D position(
        transform->position.x, transform->position.y, transform->position.z);
    const bool commander =
        entity.get_component<Engine::Core::CommanderComponent>() != nullptr;

    if (unit->owner_id != owner) {
      if (!owners.are_enemies(owner, unit->owner_id)) {
        return;
      }
      if (commander || unit->spawn_type == Game::Units::SpawnType::Barracks) {
        units.enemy_camp.push_back(position);
        return;
      }
      if (Game::Units::is_troop_spawn(unit->spawn_type) &&
          unit->spawn_type != Game::Units::SpawnType::Builder &&
          unit->spawn_type != Game::Units::SpawnType::Civilian) {
        units.enemy_troops.push_back(position);
      }
      return;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      units.barracks.push_back(position);
      if (!units.has_anchor) {
        units.anchor = position;
        units.has_anchor = true;
      }
      return;
    }
    if (commander) {
      units.commander.push_back(position);
      return;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Builder) {
      units.builders.push_back(position);
      return;
    }
    if (Game::Units::is_building_spawn(unit->spawn_type) ||
        unit->spawn_type == Game::Units::SpawnType::Civilian) {
      return;
    }
    if (Game::Units::is_troop_spawn(unit->spawn_type)) {
      units.troops.push_back(position);
    }
  });

  if (!units.has_anchor) {
    if (!units.commander.empty()) {
      units.anchor = units.commander.front();
      units.has_anchor = true;
    } else if (!units.troops.empty()) {
      units.anchor = units.troops.front();
      units.has_anchor = true;
    } else if (!units.builders.empty()) {
      units.anchor = units.builders.front();
      units.has_anchor = true;
    }
  }
  return units;
}

auto to_points(const std::vector<QVector3D>& positions, int limit) -> QVariantList {
  QVariantList list;
  const int count = std::min(static_cast<int>(positions.size()), limit);
  for (int i = 0; i < count; ++i) {
    list.append(to_point(positions[static_cast<std::size_t>(i)]));
  }
  return list;
}

} // namespace

auto resolve_tutorial_focus_points(const TutorialFocusInputs& inputs) -> QVariantList {
  using Game::Mission::TutorialFocusTarget;
  if (inputs.target == TutorialFocusTarget::None) {
    return {};
  }
  if (inputs.target == TutorialFocusTarget::WaveEntry) {
    QVariantList points;
    for (const auto& value : inputs.wave_alerts) {
      const QVariantMap alert = value.toMap();
      if (!alert.contains(QStringLiteral("x"))) {
        continue;
      }
      points.append(to_point(QVector3D(alert.value(QStringLiteral("x")).toFloat(),
                                       0.0F,
                                       alert.value(QStringLiteral("z")).toFloat())));
      if (points.size() >= k_max_focus_points) {
        break;
      }
    }
    return points;
  }

  if (inputs.world == nullptr) {
    return {};
  }
  const OwnedUnits units = collect_units(inputs.world, inputs.local_owner_id);

  switch (inputs.target) {
  case TutorialFocusTarget::OwnTroops:
    return to_points(units.troops, k_max_focus_points);

  case TutorialFocusTarget::Builders:
    return to_points(units.builders, 3);

  case TutorialFocusTarget::Barracks:
    return to_points(units.barracks, 2);

  case TutorialFocusTarget::Commander:
    return to_points(units.commander, 1);

  case TutorialFocusTarget::EnemyCamp:
    return to_points(units.enemy_camp, 3);

  case TutorialFocusTarget::EnemyScouts: {
    if (!units.has_anchor) {
      return to_points(units.enemy_troops, 3);
    }
    std::vector<QVector3D> near_scouts;
    for (const auto& position : units.enemy_troops) {
      const float dx = position.x() - units.anchor.x();
      const float dz = position.z() - units.anchor.z();
      if (dx * dx + dz * dz <= k_scout_search_radius * k_scout_search_radius) {
        near_scouts.push_back(position);
      }
    }
    return to_points(near_scouts.empty() ? units.enemy_troops : near_scouts, 3);
  }

  case TutorialFocusTarget::Timber:
    if (!units.has_anchor) {
      return {};
    }
    return to_points(nearest_props(units.anchor, &is_tree, 3), 3);

  case TutorialFocusTarget::StoneAndIron: {
    if (!units.has_anchor) {
      return {};
    }
    QVariantList points = to_points(nearest_props(units.anchor, &is_boulder, 2), 2);
    points.append(to_points(nearest_props(units.anchor, &is_iron_ore, 2), 2));
    return points;
  }

  case TutorialFocusTarget::None:
  case TutorialFocusTarget::WaveEntry:
    break;
  }
  return {};
}

} // namespace App::Mission
