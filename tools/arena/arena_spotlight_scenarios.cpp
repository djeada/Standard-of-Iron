#include "arena_spotlight_scenarios.h"

#include <QSet>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

#include "game/map/campaign_loader.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/mission_loader.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "utils/resource_utils.h"

namespace Arena::Scenarios {
namespace {

using Nation = Game::Systems::NationID;

constexpr const char* k_campaign = ":/assets/campaigns/second_punic_war.json";
constexpr int k_player_owner = 1;
constexpr float k_spotlight_seconds = 90.0F;
constexpr float k_earliest_hour = 7.5F;
constexpr float k_battle_closing_distance = 44.0F;
constexpr float k_file_spacing = 7.0F;
constexpr float k_rank_spacing = 8.0F;
const QSet<QString> k_battle_missions = {QStringLiteral("battle_of_cannae"),
                                         QStringLiteral("battle_of_zama")};
constexpr float k_latest_hour = 17.0F;

auto grid_to_world(float grid, int size, float tile) -> float {
  return (grid - (static_cast<float>(size) * 0.5F - 0.5F)) * tile;
}

auto spotlight_map(const QString& mission_id,
                   const QString& mission_title,
                   const QString& map_path) -> std::optional<ArenaScenarioDefinition> {
  Game::Map::MapDefinition map;
  QString error;
  if (!Game::Map::MapLoader::load_from_json_file(
          Utils::Resources::resolve_resource_path(map_path), map, &error)) {
    qWarning() << "Arena: cannot read spotlight map" << map_path << ":" << error;
    return std::nullopt;
  }

  ArenaScenarioDefinition s;
  s.id = QStringLiteral("spotlight_%1").arg(mission_id);
  s.label = QStringLiteral("Spotlight: %1").arg(mission_title);
  s.description =
      QStringLiteral("The production campaign map for %1, loaded with its own "
                     "terrain, environment, structures and starting armies, "
                     "for filming establishing shots.")
          .arg(mission_title);
  s.campaign_map_path = map_path;
  s.duration_seconds = k_spotlight_seconds;
  s.terrain_grid_extent = map.grid.width;
  s.arena_floor_half_extent = static_cast<float>(map.grid.width) * 0.5F;
  s.environment = map.environment;
  s.environment.start_time =
      std::clamp(map.environment.start_time, k_earliest_hour, k_latest_hour);
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = false;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  const float tile = std::max(0.0001F, map.grid.tile_size);
  const bool grid_coords = map.coordSystem == Game::Map::CoordSystem::Grid;
  const auto to_world = [&map, tile, grid_coords](float x, float z) {
    if (!grid_coords) {
      return QVector3D(x, 0.0F, z);
    }
    return QVector3D(grid_to_world(x, map.grid.width, tile),
                     0.0F,
                     grid_to_world(z, map.grid.height, tile));
  };
  s.camera = {map.camera.distance, map.camera.tilt_deg, map.camera.yaw_deg};
  s.camera_focus = to_world(map.camera.center.x(), map.camera.center.z());

  QVector3D player_center;
  QVector3D enemy_center;
  int player_count = 0;
  int enemy_count = 0;
  for (const auto& spawn : map.spawns) {
    const QVector3D position = to_world(spawn.x, spawn.z);
    if (spawn.player_id == k_player_owner) {
      player_center += position;
      ++player_count;
    } else if (spawn.player_id > 0) {
      enemy_center += position;
      ++enemy_count;
    }
  }
  player_center /= static_cast<float>(std::max(1, player_count));
  enemy_center /= static_cast<float>(std::max(1, enemy_count));

  std::vector<int> owners;
  int index = 0;
  for (const auto& spawn : map.spawns) {
    if (spawn.player_id <= 0 || Game::Units::is_wildlife_spawn(spawn.type)) {
      continue;
    }
    ArenaScenarioGroup group;
    group.name = QStringLiteral("owner%1_%2_%3")
                     .arg(spawn.player_id)
                     .arg(index++)
                     .arg(Game::Units::spawn_typeToQString(spawn.type));
    group.owner_id = spawn.player_id;
    group.nation_id = spawn.nation.value_or(
        spawn.player_id == k_player_owner ? Nation::Carthage : Nation::RomanRepublic);
    group.origin = to_world(spawn.x, spawn.z);
    if (Game::Units::is_building_spawn(spawn.type)) {
      group.spawn_type = spawn.type;
      group.max_population = spawn.max_population;
    } else if (const auto troop = Game::Units::spawn_typeToTroopType(spawn.type)) {
      group.troop_type = *troop;
    } else {
      continue;
    }
    const QVector3D facing =
        (spawn.player_id == k_player_owner ? enemy_center : player_center) -
        group.origin;
    group.facing_degrees =
        std::atan2(facing.x(), facing.z()) * 180.0F / std::numbers::pi_v<float>;
    if (std::find(owners.begin(), owners.end(), spawn.player_id) == owners.end()) {
      owners.push_back(spawn.player_id);
    }
    s.groups.push_back(std::move(group));
  }

  for (const int owner : owners) {
    s.owner_teams.push_back({owner, owner == k_player_owner ? 1 : 2});
  }
  if (!s.groups.empty()) {
    ArenaExpectation exists;
    exists.kind = ArenaExpectationKind::GroupExists;
    exists.group = s.groups.front().name;
    s.expectations.push_back(exists);
  }
  return s;
}

auto is_mounted(Game::Units::TroopType type) -> bool {
  using Troop = Game::Units::TroopType;
  return type == Troop::MountedSwordsman || type == Troop::HorseArcher ||
         type == Troop::HorseSpearman || type == Troop::Elephant;
}

auto is_missile(Game::Units::TroopType type) -> bool {
  using Troop = Game::Units::TroopType;
  return type == Troop::Archer || type == Troop::Catapult || type == Troop::Ballista;
}

auto center_of(const std::vector<const ArenaScenarioGroup*>& groups) -> QVector3D {
  QVector3D sum;
  for (const auto* group : groups) {
    sum += group->origin;
  }
  return sum / static_cast<float>(std::max<std::size_t>(1, groups.size()));
}

auto spotlight_battle(ArenaScenarioDefinition scenario,
                      float closing_distance) -> ArenaScenarioDefinition {
  std::map<int, int> troops_by_owner;
  for (const auto& group : scenario.groups) {
    if (!group.spawn_type.has_value() && group.owner_id != k_player_owner) {
      ++troops_by_owner[group.owner_id];
    }
  }
  int enemy_owner = 0;
  int enemy_troops = 0;
  for (const auto& [owner, count] : troops_by_owner) {
    if (count > enemy_troops) {
      enemy_owner = owner;
      enemy_troops = count;
    }
  }

  std::vector<ArenaScenarioGroup*> player;
  std::vector<ArenaScenarioGroup*> enemy;
  for (auto& group : scenario.groups) {
    if (group.spawn_type.has_value()) {
      continue;
    }
    if (group.owner_id == k_player_owner) {
      player.push_back(&group);
    } else if (group.owner_id == enemy_owner) {
      enemy.push_back(&group);
    }
  }
  const QVector3D player_center = center_of({player.begin(), player.end()});
  const QVector3D enemy_center = center_of({enemy.begin(), enemy.end()});
  QVector3D axis = enemy_center - player_center;
  const float gap = axis.length();
  axis = gap > 0.001F ? axis / gap : QVector3D(1.0F, 0.0F, 0.0F);
  const QVector3D across(-axis.z(), 0.0F, axis.x());
  const QVector3D middle = (player_center + enemy_center) * 0.5F;
  const auto draw_up = [&across](std::vector<ArenaScenarioGroup*>& army,
                                 QVector3D front_center,
                                 QVector3D toward_enemy) {
    std::vector<ArenaScenarioGroup*> infantry;
    std::vector<ArenaScenarioGroup*> support;
    std::vector<ArenaScenarioGroup*> horse;
    for (auto* group : army) {
      if (is_mounted(group->troop_type)) {
        horse.push_back(group);
      } else if (is_missile(group->troop_type) ||
                 Game::Units::is_commander_troop(group->troop_type) ||
                 group->troop_type == Game::Units::TroopType::Healer ||
                 group->troop_type == Game::Units::TroopType::Builder) {
        support.push_back(group);
      } else {
        infantry.push_back(group);
      }
    }
    const int files = std::max(
        4, static_cast<int>(std::ceil(static_cast<float>(infantry.size()) / 2.0F)));
    const auto place = [&](std::vector<ArenaScenarioGroup*>& rank_groups,
                           float depth,
                           int per_rank) {
      for (std::size_t index = 0; index < rank_groups.size(); ++index) {
        const int rank = static_cast<int>(index) / per_rank;
        const int file = static_cast<int>(index) % per_rank;
        const int in_rank = std::min<int>(
            per_rank, static_cast<int>(rank_groups.size()) - rank * per_rank);
        const float offset =
            (static_cast<float>(file) - (static_cast<float>(in_rank) - 1.0F) * 0.5F) *
            k_file_spacing;
        rank_groups[index]->origin = front_center + across * offset -
                                     toward_enemy * (depth + rank * k_rank_spacing);
      }
    };
    place(infantry, 0.0F, files);
    const float behind =
        k_rank_spacing *
        (1.0F + std::ceil(static_cast<float>(infantry.size()) / files));
    place(support, behind, files);
    const float wing = (static_cast<float>(files) * 0.5F + 1.5F) * k_file_spacing;
    for (std::size_t index = 0; index < horse.size(); ++index) {
      const float side = index % 2 == 0 ? -1.0F : 1.0F;
      const float step = static_cast<float>(index / 2) * k_file_spacing;
      horse[index]->origin = front_center + across * side * (wing + step);
    }
    for (auto* group : army) {
      group->facing_degrees = std::atan2(toward_enemy.x(), toward_enemy.z()) * 180.0F /
                              std::numbers::pi_v<float>;
    }
  };
  draw_up(player, middle - axis * (closing_distance * 0.5F), axis);
  draw_up(enemy, middle + axis * (closing_distance * 0.5F), -axis);

  const auto nearest = [](const ArenaScenarioGroup& from,
                          const std::vector<ArenaScenarioGroup*>& candidates) {
    const ArenaScenarioGroup* best = nullptr;
    float best_distance = std::numeric_limits<float>::max();
    for (const auto* candidate : candidates) {
      const float distance = (candidate->origin - from.origin).lengthSquared();
      if (distance < best_distance) {
        best = candidate;
        best_distance = distance;
      }
    }
    return best;
  };
  const auto order = [&scenario](const ArenaScenarioGroup& from,
                                 const ArenaScenarioGroup& target) {
    ArenaScenarioStep step;
    step.name = QStringLiteral("advance_%1").arg(from.name);
    step.trigger = {ScenarioTriggerKind::AtTime, 0.5F, {}, {}, 0.0F};
    step.command = ScenarioCommandKind::AttackMove;
    step.group = from.name;
    step.target_group = target.name;
    scenario.steps.push_back(step);
  };
  for (const auto* group : player) {
    if (const auto* target = nearest(*group, enemy)) {
      order(*group, *target);
    }
  }
  for (const auto* group : enemy) {
    if (const auto* target = nearest(*group, player)) {
      order(*group, *target);
    }
  }

  scenario.id += QStringLiteral("_battle");
  scenario.label += QStringLiteral(" (battle)");
  scenario.description +=
      QStringLiteral(" The Carthaginian host and the largest opposing army are "
                     "closed to %1 m and ordered into each other.")
          .arg(closing_distance, 0, 'f', 0);
  scenario.camera_focus = (player_center + enemy_center) * 0.5F;
  scenario.battle_sides = {
      {k_player_owner, QStringLiteral("Carthage"), player_center, 60.0F},
      {enemy_owner, QStringLiteral("Rome"), enemy_center, 60.0F}};
  return scenario;
}

} // namespace

auto build_spotlight_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  if (!Game::Campaign::CampaignLoader::load_from_json_file(
          Utils::Resources::resolve_resource_path(QString::fromLatin1(k_campaign)),
          campaign,
          &error)) {
    qWarning() << "Arena: cannot read the Barcid Road campaign:" << error;
    return result;
  }
  std::stable_sort(campaign.missions.begin(),
                   campaign.missions.end(),
                   [](const auto& lhs, const auto& rhs) {
                     return lhs.order_index < rhs.order_index;
                   });
  for (const auto& entry : campaign.missions) {
    Game::Mission::MissionDefinition mission;
    if (!Game::Mission::MissionLoader::load_from_json_file(
            Utils::Resources::resolve_resource_path(
                QStringLiteral(":/assets/missions/%1.json").arg(entry.mission_id)),
            mission,
            &error)) {
      qWarning() << "Arena: cannot read spotlight mission" << entry.mission_id << ":"
                 << error;
      continue;
    }
    if (auto scenario =
            spotlight_map(entry.mission_id, mission.title, mission.map_path)) {
      if (k_battle_missions.contains(entry.mission_id)) {
        result.push_back(spotlight_battle(*scenario, k_battle_closing_distance));
      }
      result.push_back(std::move(*scenario));
    }
  }
  return result;
}

} // namespace Arena::Scenarios
