#include "builder_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../../../map/terrain_service.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

constexpr const char* BUILDING_TYPE_HOME = "home";
constexpr const char* BUILDING_TYPE_DEFENSE_TOWER = "defense_tower";
constexpr const char* BUILDING_TYPE_BARRACKS = "barracks";
constexpr const char* BUILDING_TYPE_CATAPULT = "catapult";

constexpr int MAX_HOMES = 20;
constexpr int MAX_DEFENSE_TOWERS = 10;
constexpr int MAX_BARRACKS = 6;
constexpr int MAX_CATAPULTS = 5;

constexpr float DEFENSE_TOWER_CLOSE_RADIUS = 25.0F;
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

  if (context.outpost_barracks_count < context.strategy_config.desired_outpost_barracks_count) {
    return true;
  }

  return context.outpost_barracks_count >=
             context.strategy_config.desired_outpost_barracks_count &&
         context.outpost_home_count < context.strategy_config.outpost_home_target;
}

auto recent_outpost_order(const AISnapshot& snapshot, const AIContext& context) -> bool {
  return (snapshot.game_time - context.last_expansion_order_time) < 4.0F;
}

auto select_best_builder(const AISnapshot& snapshot,
                         const std::vector<Engine::Core::EntityID>& available_builders,
                         float target_x,
                         float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = available_builders.front();
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (auto builder_id : available_builders) {
    auto it = std::find_if(snapshot.friendly_units.begin(),
                           snapshot.friendly_units.end(),
                           [builder_id](const EntitySnapshot& entity) {
                             return entity.id == builder_id;
                           });
    if (it == snapshot.friendly_units.end()) {
      continue;
    }

    const float distance_sq =
        (it->pos_x - target_x) * (it->pos_x - target_x) +
        (it->pos_z - target_z) * (it->pos_z - target_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_id = builder_id;
    }
  }

  return best_id;
}

} // namespace

void BuilderBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
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
  const int target_catapults = std::clamp(targets.catapult_count, 0, MAX_CATAPULTS);

  const char* building_to_construct = nullptr;
  float construction_x = context.base_pos_x;
  float construction_z = context.base_pos_z;
  bool expansion_order = false;

  if (context.state == AIState::Expanding && needs_outpost_construction(context)) {
    if (context.expansion_construction_pending || recent_outpost_order(snapshot, context)) {
      return;
    }

    if (context.outpost_barracks_count < context.strategy_config.desired_outpost_barracks_count) {
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
                   {BUILDING_TYPE_HOME, context.home_count, target_homes},
                   {BUILDING_TYPE_CATAPULT, catapult_count, target_catapults},
               })) {
      building_to_construct = candidate;
    }

    if (building_to_construct == nullptr) {
      return;
    }

    if (context.primary_barracks != 0) {
      float angle = m_construction_counter * 0.8F;
      float radius = 15.0F + (m_construction_counter % 3) * 5.0F;

      if (building_to_construct == BUILDING_TYPE_DEFENSE_TOWER) {
        radius = std::min(radius, DEFENSE_TOWER_CLOSE_RADIUS);
      } else if (building_to_construct == BUILDING_TYPE_BARRACKS) {
        radius += 5.0F;
      }

      construction_x += radius * std::cos(angle);
      construction_z += radius * std::sin(angle);
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
  if (context.builder_count == 0) {
    return false;
  }

  const int catapult_count = static_cast<int>(std::count_if(
      snapshot.friendly_units.begin(),
      snapshot.friendly_units.end(),
      [](const EntitySnapshot& entity) {
        return !entity.is_building &&
               entity.spawn_type == Game::Units::SpawnType::Catapult;
      }));

  if (context.state == AIState::Expanding && needs_outpost_construction(context)) {
    return true;
  }

  return context.home_count < std::clamp(context.macro_targets.home_count, 2, MAX_HOMES) ||
         context.barracks_count <
             std::clamp(context.macro_targets.barracks_count, 1, MAX_BARRACKS) ||
         context.defense_tower_count <
             std::clamp(context.macro_targets.defense_tower_count, 0, MAX_DEFENSE_TOWERS) ||
         catapult_count < std::clamp(context.macro_targets.catapult_count, 0, MAX_CATAPULTS);
}

} // namespace Game::Systems::AI
