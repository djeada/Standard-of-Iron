#include "commander_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../ai_attack_wave.h"
#include "../ai_base_manager.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

struct ArmyCentre {
  float x = 0.0F;
  float z = 0.0F;
  int count = 0;
};

auto compute_army_centre(const AISnapshot& snapshot) -> ArmyCentre {
  ArmyCentre centre;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.is_commander) {
      continue;
    }
    if (entity.spawn_type == Game::Units::SpawnType::Builder) {
      continue;
    }
    centre.x += entity.pos_x;
    centre.z += entity.pos_z;
    centre.count++;
  }
  if (centre.count > 0) {
    const float inv = 1.0F / static_cast<float>(centre.count);
    centre.x *= inv;
    centre.z *= inv;
  }
  return centre;
}

void nearest_enemy_direction(const AISnapshot& snapshot,
                             float army_x,
                             float army_z,
                             float& out_dx,
                             float& out_dz) {
  out_dx = 0.0F;
  out_dz = 1.0F;

  float nearest_sq = std::numeric_limits<float>::infinity();
  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building) {
      continue;
    }
    const float dx = enemy.pos_x - army_x;
    const float dz = enemy.pos_z - army_z;
    const float dist_sq = dx * dx + dz * dz;
    if (dist_sq < nearest_sq) {
      nearest_sq = dist_sq;
      out_dx = dx;
      out_dz = dz;
    }
  }

  const float len = std::sqrt(out_dx * out_dx + out_dz * out_dz);
  if (len > 0.1F) {
    out_dx /= len;
    out_dz /= len;
  }
}

constexpr float k_rally_interval = 5.0F;
constexpr float k_aura_interval = 20.0F;

constexpr float k_shielded_offset = -8.0F;
constexpr float k_protected_offset = -5.0F;
constexpr float k_leading_offset = -3.0F;

constexpr float k_flanking_lateral_offset = 7.0F;

struct CommanderStation {
  float along_axis{0.0F};
  float lateral{0.0F};
};

auto station_for(const AIStrategyConfig& config) -> CommanderStation {
  switch (config.strategy) {
  case AIStrategy::Aggressive:
  case AIStrategy::Rusher:
    return {k_leading_offset, 0.0F};
  case AIStrategy::Harasser:
    return {0.0F, k_flanking_lateral_offset};
  case AIStrategy::Defensive:
  case AIStrategy::Economic:
  case AIStrategy::SepulcherDefense:
    return {k_shielded_offset, 0.0F};
  case AIStrategy::Balanced:
  case AIStrategy::Expansionist:
    break;
  }
  return {k_protected_offset, 0.0F};
}

auto rally_interval_for(const AIStrategyConfig& config) -> float {
  float const scale = std::clamp(config.defense_modifier, 0.5F, 2.0F);
  return k_rally_interval / scale;
}

auto aura_interval_for(const AIStrategyConfig& config) -> float {
  float const scale = std::clamp(config.aggression_modifier, 0.5F, 2.0F);
  return k_aura_interval / scale;
}

constexpr float k_aura_engagement_radius = 16.0F;

constexpr float k_commander_march_health = 0.6F;
constexpr float k_home_ground_radius = AIBaseManager::k_base_defend_radius;
constexpr float k_home_ground_radius_sq = k_home_ground_radius * k_home_ground_radius;

auto leads_from_the_front(const AIStrategyConfig& config) -> bool {
  switch (config.strategy) {
  case AIStrategy::Aggressive:
  case AIStrategy::Rusher:
  case AIStrategy::Harasser:
    return true;
  default:
    return false;
  }
}

constexpr int k_minimum_escort = 2;

auto commander_marches_with_army(const AIContext& context,
                                 const EntitySnapshot& commander,
                                 const ArmyCentre& army) -> bool {
  if (army.count < k_minimum_escort) {
    return false;
  }

  const float health_fraction = commander.max_health > 0
                                    ? static_cast<float>(commander.health) /
                                          static_cast<float>(commander.max_health)
                                    : 1.0F;
  if (health_fraction < k_commander_march_health) {
    return false;
  }

  if (!context.has_base_anchor) {
    return true;
  }

  const float dx = army.x - context.base_pos_x;
  const float dz = army.z - context.base_pos_z;
  if ((dx * dx + dz * dz) <= k_home_ground_radius_sq) {
    return true;
  }

  if (!leads_from_the_front(context.strategy_config) || !context.wave.committed) {
    return false;
  }
  const int escort = static_cast<int>(context.wave.members.size());
  if (escort < std::max(k_minimum_escort, wave_size_for(context) - 1)) {
    return false;
  }
  return context.combat_units >= escort + k_minimum_escort;
}

auto enemies_are_within(const AISnapshot& snapshot,
                        float centre_x,
                        float centre_z,
                        float radius) -> bool {
  float const radius_sq = radius * radius;
  return std::any_of(snapshot.visible_enemies.begin(),
                     snapshot.visible_enemies.end(),
                     [&](const ContactSnapshot& enemy) {
                       if (enemy.health <= 0) {
                         return false;
                       }
                       float const dx = enemy.pos_x - centre_x;
                       float const dz = enemy.pos_z - centre_z;
                       return (dx * dx + dz * dz) <= radius_sq;
                     });
}

} // namespace

void CommanderBehavior::execute(const AISnapshot& snapshot,
                                AIContext& context,
                                float delta_time,
                                std::vector<AICommand>& out_commands) {
  m_update_timer += delta_time;
  m_rally_timer += delta_time;
  m_aura_timer += delta_time;

  const ArmyCentre army = compute_army_centre(snapshot);
  const auto& config = context.strategy_config;

  if (m_rally_timer >= rally_interval_for(config)) {
    m_rally_timer = 0.0F;
    for (auto commander_id : context.commander_ids) {
      AICommand rally_cmd;
      rally_cmd.type = AICommandType::TriggerCommanderRally;
      rally_cmd.units = {commander_id};
      out_commands.push_back(std::move(rally_cmd));
    }
  }

  const bool fighting_is_close =
      army.count > 0 &&
      enemies_are_within(snapshot, army.x, army.z, k_aura_engagement_radius);
  if (m_aura_timer >= aura_interval_for(config) && fighting_is_close) {
    m_aura_timer = 0.0F;
    for (auto commander_id : context.commander_ids) {
      AICommand aura_cmd;
      aura_cmd.type = AICommandType::TriggerCommanderAura;
      aura_cmd.units = {commander_id};
      out_commands.push_back(std::move(aura_cmd));
    }
  }

  if (m_update_timer < k_update_interval) {
    return;
  }
  m_update_timer = 0.0F;

  float enemy_dir_x = 0.0F;
  float enemy_dir_z = 1.0F;
  if (!snapshot.visible_enemies.empty() && army.count > 0) {
    nearest_enemy_direction(snapshot, army.x, army.z, enemy_dir_x, enemy_dir_z);
  }

  for (auto commander_id : context.commander_ids) {
    const EntitySnapshot* snap = nullptr;
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.id == commander_id) {
        snap = &entity;
        break;
      }
    }
    if (snap == nullptr) {
      continue;
    }

    float target_x;
    float target_z;

    if (commander_marches_with_army(context, *snap, army)) {
      const CommanderStation station = station_for(config);
      target_x =
          army.x + enemy_dir_x * station.along_axis - enemy_dir_z * station.lateral;
      target_z =
          army.z + enemy_dir_z * station.along_axis + enemy_dir_x * station.lateral;
    } else if (context.has_base_anchor) {
      target_x = context.rally_x;
      target_z = context.rally_z;
    } else {
      continue;
    }

    const float dx = snap->pos_x - target_x;
    const float dz = snap->pos_z - target_z;
    if (dx * dx + dz * dz < k_snap_threshold_sq) {
      continue;
    }

    auto claimed = claim_units({commander_id},
                               get_priority(),
                               "commander_positioning",
                               context,
                               snapshot.game_time,
                               1.5F);
    if (claimed.empty()) {
      continue;
    }

    AICommand move_cmd;
    move_cmd.type = AICommandType::MoveUnits;
    move_cmd.units = std::move(claimed);
    move_cmd.move_target_x = {target_x};
    move_cmd.move_target_y = {0.0F};
    move_cmd.move_target_z = {target_z};
    out_commands.push_back(std::move(move_cmd));
  }
}

auto CommanderBehavior::should_execute(const AISnapshot& snapshot,
                                       const AIContext& context) const -> bool {
  (void)snapshot;
  return !context.commander_ids.empty();
}

} // namespace Game::Systems::AI
