#include "app/commander/commander_bearings.h"

#include <cmath>
#include <limits>

#include "game/commander/commander_heading.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/core/world_spatial_index.h"
#include "game/map/visibility_service.h"
#include "game/systems/owner_registry.h"
#include "game/util/asset_text.h"

namespace App::Core {

using Game::Commander::k_radians_to_degrees;
using Game::Commander::signed_angle_delta;

namespace {

constexpr float k_own_army_minimum_distance = 14.0F;
constexpr float k_rally_flag_minimum_distance = 6.0F;

auto bearing_to(float from_x, float from_z, float to_x, float to_z, float view_yaw)
    -> float {
  float const yaw_to = std::atan2(to_x - from_x, to_z - from_z) * k_radians_to_degrees;

  return -signed_angle_delta(yaw_to, view_yaw);
}

} // namespace

auto commander_bearing_kind_name(CommanderBearing::Kind kind) -> const char* {
  switch (kind) {
  case CommanderBearing::Kind::EnemyCommander:
    return "enemy_commander";
  case CommanderBearing::Kind::Enemy:
    return "enemy";
  case CommanderBearing::Kind::Army:
    return "army";
  case CommanderBearing::Kind::RallyFlag:
    return "rally";
  }
  return "enemy";
}

auto collect_commander_bearings(const CommanderBearingsInput& input)
    -> std::vector<CommanderBearing> {
  std::vector<CommanderBearing> bearings;
  if (input.world == nullptr || input.owners == nullptr || input.commander_id == 0) {
    return bearings;
  }
  auto const& world = *input.world;
  auto const* own = world.try_get<Engine::Core::TransformComponent>(input.commander_id);
  if (own == nullptr) {
    return bearings;
  }
  float const x = own->position.x;
  float const z = own->position.z;
  auto const seen = [&input](float world_x, float world_z) {
    return input.visibility == nullptr || !input.visibility->is_initialized() ||
           input.visibility->is_visible_world(world_x, world_z);
  };
  auto const mark = [&](CommanderBearing::Kind kind, float to_x, float to_z) {
    CommanderBearing bearing;
    bearing.kind = kind;
    bearing.right_degrees = bearing_to(x, z, to_x, to_z, input.view_yaw_degrees);
    bearing.distance = std::hypot(to_x - x, to_z - z);
    return bearing;
  };

  using Index = Engine::Core::WorldSpatialIndex;
  float nearest_enemy_sq = std::numeric_limits<float>::max();
  const Index::Entry* nearest_enemy = nullptr;
  double army_x = 0.0;
  double army_z = 0.0;
  int army_count = 0;
  for (auto const& entry : world.spatial_index().entries()) {
    if (entry.id == input.commander_id || entry.health <= 0 ||
        !entry.is(Index::k_alive) ||
        entry.is(Index::k_building | Index::k_wildlife | Index::k_pending_removal)) {
      continue;
    }
    if (entry.owner_id == input.local_owner_id) {
      army_x += entry.x;
      army_z += entry.z;
      ++army_count;
      continue;
    }
    if (!input.owners->are_enemies(input.local_owner_id, entry.owner_id) ||
        !seen(entry.x, entry.z)) {
      continue;
    }
    if (auto const* commander =
            world.try_get<Engine::Core::CommanderComponent>(entry.id)) {
      auto bearing = mark(CommanderBearing::Kind::EnemyCommander, entry.x, entry.z);
      if (!commander->display_name.empty()) {
        bearing.label = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                             commander->display_name);
      }
      bearings.push_back(std::move(bearing));
      continue;
    }
    float const distance_sq =
        ((entry.x - x) * (entry.x - x)) + ((entry.z - z) * (entry.z - z));
    if (distance_sq < nearest_enemy_sq) {
      nearest_enemy_sq = distance_sq;
      nearest_enemy = &entry;
    }
  }
  if (nearest_enemy != nullptr) {
    bearings.push_back(
        mark(CommanderBearing::Kind::Enemy, nearest_enemy->x, nearest_enemy->z));
  }
  if (army_count > 0) {
    auto bearing = mark(CommanderBearing::Kind::Army,
                        static_cast<float>(army_x / army_count),
                        static_cast<float>(army_z / army_count));
    if (bearing.distance >= k_own_army_minimum_distance) {
      bearings.push_back(std::move(bearing));
    }
  }
  if (auto const* commander =
          world.try_get<Engine::Core::CommanderComponent>(input.commander_id);
      commander != nullptr && commander->flag_rally_flag_active) {
    auto bearing = mark(CommanderBearing::Kind::RallyFlag,
                        commander->flag_rally_flag_x,
                        commander->flag_rally_flag_z);
    if (bearing.distance >= k_rally_flag_minimum_distance) {
      bearings.push_back(std::move(bearing));
    }
  }
  return bearings;
}

} // namespace App::Core
