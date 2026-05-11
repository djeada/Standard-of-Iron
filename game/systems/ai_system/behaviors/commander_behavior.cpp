#include "commander_behavior.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

#include <cmath>
#include <limits>
#include <vector>

namespace Game::Systems::AI {

namespace {

struct ArmyCentre {
  float x = 0.0F;
  float z = 0.0F;
  int count = 0;
};

auto compute_army_centre(const AISnapshot &snapshot) -> ArmyCentre {
  ArmyCentre centre;
  for (const auto &entity : snapshot.friendly_units) {
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

// Returns the unit direction vector pointing from the army centre toward the
// nearest non-building enemy, or {0,1} if there are no enemies.
void nearest_enemy_direction(const AISnapshot &snapshot, float army_x,
                             float army_z, float &out_dx, float &out_dz) {
  out_dx = 0.0F;
  out_dz = 1.0F;

  float nearest_sq = std::numeric_limits<float>::infinity();
  for (const auto &enemy : snapshot.visible_enemies) {
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

} // namespace

void CommanderBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                                float delta_time,
                                std::vector<AICommand> &out_commands) {
  m_update_timer += delta_time;
  m_rally_timer += delta_time;

  // Periodically trigger rally on all active commanders regardless of
  // movement update cadence.
  if (m_rally_timer >= k_rally_interval) {
    m_rally_timer = 0.0F;
    for (auto commander_id : context.commander_ids) {
      AICommand rally_cmd;
      rally_cmd.type = AICommandType::TriggerCommanderRally;
      rally_cmd.units = {commander_id};
      out_commands.push_back(std::move(rally_cmd));
    }
  }

  if (m_update_timer < k_update_interval) {
    return;
  }
  m_update_timer = 0.0F;

  const ArmyCentre army = compute_army_centre(snapshot);

  float enemy_dir_x = 0.0F;
  float enemy_dir_z = 1.0F;
  if (!snapshot.visible_enemies.empty() && army.count > 0) {
    nearest_enemy_direction(snapshot, army.x, army.z, enemy_dir_x,
                            enemy_dir_z);
  }

  for (auto commander_id : context.commander_ids) {
    const EntitySnapshot *snap = nullptr;
    for (const auto &entity : snapshot.friendly_units) {
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

    if (army.count > 0) {
      // Keep commander at army centre, offset slightly away from the enemy to
      // protect them while still letting the aura reach the front line.
      target_x = army.x - enemy_dir_x * k_protected_offset;
      target_z = army.z - enemy_dir_z * k_protected_offset;
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

    auto claimed =
        claim_units({commander_id}, get_priority(), "commander_positioning",
                    context, m_update_timer + delta_time, 1.5F);
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

auto CommanderBehavior::should_execute(const AISnapshot &snapshot,
                                       const AIContext &context) const -> bool {
  (void)snapshot;
  return !context.commander_ids.empty();
}

} // namespace Game::Systems::AI
