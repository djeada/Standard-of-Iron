#include "builder_behavior.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../../../map/terrain_service.h"
#include "../../construction_cost_catalog.h"
#include "../../nation_registry.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

constexpr const char* BUILDING_TYPE_HOME = "home";
constexpr const char* BUILDING_TYPE_DEFENSE_TOWER = "defense_tower";
constexpr const char* BUILDING_TYPE_WALL_SEGMENT = "wall_segment";
constexpr const char* BUILDING_TYPE_BARRACKS = "barracks";
constexpr const char* BUILDING_TYPE_MARKETPLACE = "marketplace";
constexpr const char* BUILDING_TYPE_CATAPULT = "catapult";
constexpr const char* HARVEST_TREE = "cut_tree";
constexpr const char* HARVEST_STONE = "collect_stone";
constexpr const char* HARVEST_IRON = "collect_iron_ore";

constexpr int MAX_HOMES = 20;
constexpr int MAX_DEFENSE_TOWERS = 10;
constexpr int MAX_WALL_SEGMENTS = 12;
constexpr int MAX_BARRACKS = 6;
constexpr int MAX_MARKETPLACES = 2;
constexpr int MAX_CATAPULTS = 5;

constexpr float MAP_EDGE_PADDING = 5.0F;

constexpr float GRID_CENTER_OFFSET = 0.5F;

void clamp_to_map_bounds(float& x, float& z) {
  auto& terrain = Game::Map::TerrainService::instance();
  if (!terrain.is_initialized()) {
    return;
  }

  const Game::Map::TerrainHeightMap* hm = terrain.get_height_map();
  if (hm == nullptr) {
    return;
  }

  const float tile = hm->get_tile_size();
  const int w = hm->get_width();
  const int h = hm->get_height();
  if (w <= 0 || h <= 0) {
    return;
  }

  const float half_w = w * GRID_CENTER_OFFSET - GRID_CENTER_OFFSET;
  const float half_h = h * GRID_CENTER_OFFSET - GRID_CENTER_OFFSET;
  const float min_x = -half_w * tile + MAP_EDGE_PADDING;
  const float max_x = half_w * tile - MAP_EDGE_PADDING;
  const float min_z = -half_h * tile + MAP_EDGE_PADDING;
  const float max_z = half_h * tile - MAP_EDGE_PADDING;

  x = std::clamp(x, min_x, max_x);
  z = std::clamp(z, min_z, max_z);
}

struct BuildCandidate {
  const char* type = nullptr;
  int current = 0;
  int target = 0;
};

auto best_candidate(std::initializer_list<BuildCandidate> candidates) -> const char* {
  const char* best = nullptr;
  float best_completion = std::numeric_limits<float>::infinity();
  for (const auto& candidate : candidates) {
    if (candidate.type == nullptr || candidate.target <= candidate.current) {
      continue;
    }

    const float completion = static_cast<float>(candidate.current) /
                             static_cast<float>(std::max(1, candidate.target));
    if (completion < best_completion) {
      best = candidate.type;
      best_completion = completion;
    }
  }

  return best;
}

auto needs_outpost_construction(const AIContext& context) -> bool {
  if (!context.has_expansion_site) {
    return false;
  }

  if (context.outpost_barracks_count <
      context.strategy_config.desired_outpost_barracks_count) {
    return true;
  }

  return context.outpost_barracks_count >=
             context.strategy_config.desired_outpost_barracks_count &&
         context.outpost_home_count < context.strategy_config.outpost_home_target;
}

auto recent_outpost_order(const AISnapshot& snapshot,
                          const AIContext& context) -> bool {
  return (snapshot.game_time - context.last_expansion_order_time) < 4.0F;
}

auto select_best_builder(const AISnapshot& snapshot,
                         const std::vector<Engine::Core::EntityID>& available_builders,
                         float target_x,
                         float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = available_builders.front();
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (auto builder_id : available_builders) {
    auto it = std::find_if(
        snapshot.friendly_units.begin(),
        snapshot.friendly_units.end(),
        [builder_id](const EntitySnapshot& entity) { return entity.id == builder_id; });
    if (it == snapshot.friendly_units.end()) {
      continue;
    }

    const float distance_sq = (it->pos_x - target_x) * (it->pos_x - target_x) +
                              (it->pos_z - target_z) * (it->pos_z - target_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_id = builder_id;
    }
  }

  return best_id;
}

auto harvest_type_for_resource(ResourceType resource) -> const char* {
  switch (resource) {
  case ResourceType::Wood:
    return HARVEST_TREE;
  case ResourceType::Stone:
    return HARVEST_STONE;
  case ResourceType::Iron:
    return HARVEST_IRON;
  default:
    return nullptr;
  }
}

auto node_matches_resource(const ResourceNodeSnapshot& node,
                           ResourceType resource) -> bool {
  switch (resource) {
  case ResourceType::Wood:
    return Game::Map::is_tree_world_prop_type(node.type);
  case ResourceType::Stone:
    return Game::Map::is_boulder_world_prop_type(node.type);
  case ResourceType::Iron:
    return Game::Map::is_iron_ore_world_prop_type(node.type);
  default:
    return false;
  }
}

auto planned_settlement_offset(const AIContext& context,
                               const char* building_type,
                               int construction_index) -> QVector3D {
  const bool carthaginian =
      context.nation != nullptr && context.nation->id == NationID::Carthage;
  if (building_type == BUILDING_TYPE_DEFENSE_TOWER) {
    static const std::array<QVector3D, 4> roman = {QVector3D{-8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, 8.0F},
                                                   QVector3D{-8.0F, 0.0F, 8.0F}};
    static const std::array<QVector3D, 4> punic = {QVector3D{-8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, -8.0F},
                                                   QVector3D{-8.0F, 0.0F, 8.0F},
                                                   QVector3D{8.0F, 0.0F, 8.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    return offsets[static_cast<std::size_t>(construction_index) % offsets.size()];
  }
  if (building_type == BUILDING_TYPE_HOME) {
    static const std::array<QVector3D, 6> roman = {QVector3D{-8.0F, 0.0F, 5.0F},
                                                   QVector3D{-4.0F, 0.0F, 5.0F},
                                                   QVector3D{4.0F, 0.0F, 5.0F},
                                                   QVector3D{8.0F, 0.0F, 5.0F},
                                                   QVector3D{-8.0F, 0.0F, -5.0F},
                                                   QVector3D{8.0F, 0.0F, -5.0F}};
    static const std::array<QVector3D, 6> punic = {QVector3D{-6.0F, 0.0F, 4.0F},
                                                   QVector3D{-2.0F, 0.0F, 6.0F},
                                                   QVector3D{2.0F, 0.0F, 6.0F},
                                                   QVector3D{6.0F, 0.0F, 4.0F},
                                                   QVector3D{-5.0F, 0.0F, -5.0F},
                                                   QVector3D{5.0F, 0.0F, -5.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    return offsets[static_cast<std::size_t>(construction_index) % offsets.size()];
  }
  if (building_type == BUILDING_TYPE_MARKETPLACE) {
    return carthaginian ? QVector3D{0.0F, 0.0F, 2.0F} : QVector3D{};
  }
  if (building_type == BUILDING_TYPE_BARRACKS) {
    return QVector3D{0.0F, 0.0F, carthaginian ? -7.0F : -8.0F};
  }
  if (building_type == BUILDING_TYPE_WALL_SEGMENT) {
    const int slot = construction_index % 11;
    return QVector3D{
        -10.0F + static_cast<float>(slot) * 2.0F, 0.0F, carthaginian ? -13.0F : -14.0F};
  }
  const float angle = static_cast<float>(construction_index) * 0.8F;
  return {18.0F * std::cos(angle), 0.0F, 18.0F * std::sin(angle)};
}

} // namespace

void BuilderBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return;
  }

  m_construction_timer += delta_time;
  if (m_construction_timer < 3.0F) {
    return;
  }
  m_construction_timer = 0.0F;

  std::vector<Engine::Core::EntityID> available_builders;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }

    if (entity.builder_production.has_component &&
        entity.builder_production.has_construction_site) {
      continue;
    }

    if (entity.movement.has_component && !entity.movement.has_target) {
      available_builders.push_back(entity.id);
    }
  }

  if (available_builders.empty()) {
    return;
  }

  int catapult_count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type == Game::Units::SpawnType::Catapult) {
      catapult_count++;
    }
  }

  const auto& targets = context.macro_targets;
  const int target_homes = std::clamp(targets.home_count, 2, MAX_HOMES);
  const int target_barracks = std::clamp(targets.barracks_count, 1, MAX_BARRACKS);
  const int target_towers =
      std::clamp(targets.defense_tower_count, 0, MAX_DEFENSE_TOWERS);
  const int target_walls = std::clamp(targets.wall_segment_count, 0, MAX_WALL_SEGMENTS);
  const int target_catapults = std::clamp(targets.catapult_count, 0, MAX_CATAPULTS);

  const char* building_to_construct = nullptr;
  float construction_x = context.base_pos_x;
  float construction_z = context.base_pos_z;
  bool expansion_order = false;

  if (context.state == AIState::Expanding && needs_outpost_construction(context)) {
    if (context.expansion_construction_pending ||
        recent_outpost_order(snapshot, context)) {
      return;
    }

    if (context.outpost_barracks_count <
        context.strategy_config.desired_outpost_barracks_count) {
      building_to_construct = BUILDING_TYPE_BARRACKS;
      construction_x = context.expansion_site_x;
      construction_z = context.expansion_site_z;
    } else {
      building_to_construct = BUILDING_TYPE_HOME;
      const float dx = context.expansion_site_x - context.base_pos_x;
      const float dz = context.expansion_site_z - context.base_pos_z;
      const float dist = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
      const float offset_scale = (dist > 0.1F) ? (8.0F / dist) : 0.0F;
      construction_x = context.expansion_site_x - dz * offset_scale;
      construction_z = context.expansion_site_z + dx * offset_scale;
    }
    expansion_order = true;
  } else {
    if (context.home_count < 2) {
      building_to_construct = BUILDING_TYPE_HOME;
    } else if (context.barracks_count == 0) {
      building_to_construct = BUILDING_TYPE_BARRACKS;
    } else if (context.barracks_under_threat &&
               context.defense_tower_count < target_towers) {
      building_to_construct = BUILDING_TYPE_DEFENSE_TOWER;
    } else if (const char* candidate = best_candidate({
                   {BUILDING_TYPE_BARRACKS, context.barracks_count, target_barracks},
                   {BUILDING_TYPE_DEFENSE_TOWER,
                    context.defense_tower_count,
                    target_towers},
                   {BUILDING_TYPE_MARKETPLACE, context.marketplace_count, 1},
                   {BUILDING_TYPE_WALL_SEGMENT,
                    context.wall_segment_count,
                    target_walls},
                   {BUILDING_TYPE_HOME, context.home_count, target_homes},
                   {BUILDING_TYPE_CATAPULT, catapult_count, target_catapults},
               })) {
      building_to_construct = candidate;
    }

    if (building_to_construct == nullptr) {
      return;
    }

    const auto costs = construction_cost_info(building_to_construct).resource_costs;
    ResourceType missing_resource = ResourceType::Count;
    int largest_deficit = 0;
    if (snapshot.has_resource_snapshot) {
      for (const ResourceType type : k_all_resource_types) {
        const int deficit = costs.get(type) - snapshot.resources.get(type);
        if (deficit > largest_deficit && harvest_type_for_resource(type) != nullptr) {
          missing_resource = type;
          largest_deficit = deficit;
        }
      }
    }

    if (missing_resource != ResourceType::Count) {
      const ResourceNodeSnapshot* closest = nullptr;
      float closest_distance_sq = std::numeric_limits<float>::infinity();
      for (const auto& node : snapshot.resource_nodes) {
        if (node.reserved || !node_matches_resource(node, missing_resource)) {
          continue;
        }
        const float dx = node.pos_x - context.base_pos_x;
        const float dz = node.pos_z - context.base_pos_z;
        const float distance_sq = dx * dx + dz * dz;
        if (distance_sq < closest_distance_sq) {
          closest = &node;
          closest_distance_sq = distance_sq;
        }
      }
      if (closest == nullptr) {
        return;
      }

      AICommand command;
      command.type = AICommandType::StartBuilderHarvest;
      command.units.push_back(select_best_builder(
          snapshot, available_builders, closest->pos_x, closest->pos_z));
      command.construction_type = harvest_type_for_resource(missing_resource);
      command.construction_site_x = closest->pos_x;
      command.construction_site_z = closest->pos_z;
      command.resource_target_id = closest->id;
      out_commands.push_back(std::move(command));
      return;
    }

    if (context.primary_barracks != 0) {
      const QVector3D offset = planned_settlement_offset(
          context, building_to_construct, m_construction_counter);
      construction_x += offset.x();
      construction_z += offset.z();
    }
  }

  clamp_to_map_bounds(construction_x, construction_z);

  if (!available_builders.empty()) {
    AICommand command;
    command.type = AICommandType::StartBuilderConstruction;
    command.units.push_back(select_best_builder(
        snapshot, available_builders, construction_x, construction_z));
    command.construction_type = building_to_construct;
    command.construction_site_x = construction_x;
    command.construction_site_z = construction_z;
    out_commands.push_back(std::move(command));

    if (expansion_order) {
      context.last_expansion_order_time = snapshot.game_time;
    }
    m_construction_counter++;
  }
}

auto BuilderBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }

  if (context.builder_count == 0) {
    return false;
  }

  const int catapult_count = static_cast<int>(
      std::count_if(snapshot.friendly_units.begin(),
                    snapshot.friendly_units.end(),
                    [](const EntitySnapshot& entity) {
                      return !entity.is_building &&
                             entity.spawn_type == Game::Units::SpawnType::Catapult;
                    }));

  if (context.state == AIState::Expanding && needs_outpost_construction(context)) {
    return true;
  }

  return context.home_count <
             std::clamp(context.macro_targets.home_count, 2, MAX_HOMES) ||
         context.barracks_count <
             std::clamp(context.macro_targets.barracks_count, 1, MAX_BARRACKS) ||
         context.marketplace_count <
             std::clamp(context.macro_targets.marketplace_count, 0, MAX_MARKETPLACES) ||
         context.defense_tower_count <
             std::clamp(
                 context.macro_targets.defense_tower_count, 0, MAX_DEFENSE_TOWERS) ||
         context.wall_segment_count <
             std::clamp(
                 context.macro_targets.wall_segment_count, 0, MAX_WALL_SEGMENTS) ||
         catapult_count <
             std::clamp(context.macro_targets.catapult_count, 0, MAX_CATAPULTS);
}

} // namespace Game::Systems::AI
