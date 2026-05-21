#include "expand_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../../core/ownership_constants.h"
#include "../../nation_registry.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

void ExpandBehavior::execute(const AISnapshot& snapshot,
                             AIContext& context,
                             float delta_time,
                             std::vector<AICommand>& out_commands) {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return;
  }

  m_expand_timer += delta_time;

  if (m_expand_timer < 1.0F) {
    return;
  }
  m_expand_timer = 0.0F;

  const ContactSnapshot* closest_neutral_barracks = nullptr;
  float closest_distance_sq = std::numeric_limits<float>::max();

  for (const auto& enemy : snapshot.visible_enemies) {
    if (!enemy.is_building) {
      continue;
    }

    if (enemy.spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    if (!Game::Core::is_neutral_owner(enemy.owner_id)) {
      continue;
    }

    if (context.primary_barracks != 0) {
      float const dx = enemy.pos_x - context.base_pos_x;
      float const dz = enemy.pos_z - context.base_pos_z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq < closest_distance_sq) {
        closest_distance_sq = dist_sq;
        closest_neutral_barracks = &enemy;
      }
    }
  }

  if (closest_neutral_barracks == nullptr) {
    if (!context.has_expansion_site) {
      return;
    }
  }

  std::vector<const EntitySnapshot*> available_units;
  if (closest_neutral_barracks != nullptr) {
    available_units = collect_attack_force_units(snapshot, context);
  } else {
    available_units = collect_attack_force_units(snapshot, context);
  }

  if (available_units.empty()) {
    return;
  }

  constexpr float CAPTURE_APPROACH_DISTANCE = 5.0F;

  std::vector<Engine::Core::EntityID> unit_ids;
  std::vector<float> target_x;
  std::vector<float> target_y;
  std::vector<float> target_z;

  unit_ids.reserve(available_units.size());
  target_x.reserve(available_units.size());
  target_y.reserve(available_units.size());
  target_z.reserve(available_units.size());

  for (size_t index = 0; index < available_units.size(); ++index) {
    const auto* unit = available_units[index];
    unit_ids.push_back(unit->id);

    float offset_x = 0.0F;
    float offset_z = 0.0F;

    if (closest_neutral_barracks != nullptr && context.primary_barracks != 0) {
      float const dx = unit->pos_x - closest_neutral_barracks->pos_x;
      float const dz = unit->pos_z - closest_neutral_barracks->pos_z;
      float const dist = std::sqrt(dx * dx + dz * dz);

      if (dist > 0.1F) {
        offset_x = -(dx / dist) * CAPTURE_APPROACH_DISTANCE;
        offset_z = -(dz / dist) * CAPTURE_APPROACH_DISTANCE;
      }
    } else if (closest_neutral_barracks == nullptr) {
      const float angle = static_cast<float>(index) * 0.85F;
      const float radius = 6.0F + static_cast<float>(index % 3) * 1.5F;
      offset_x = std::cos(angle) * radius;
      offset_z = std::sin(angle) * radius;
    }

    const float target_center_x = (closest_neutral_barracks != nullptr)
                                      ? closest_neutral_barracks->pos_x
                                      : context.expansion_site_x;
    const float target_center_z = (closest_neutral_barracks != nullptr)
                                      ? closest_neutral_barracks->pos_z
                                      : context.expansion_site_z;
    target_x.push_back(target_center_x + offset_x);
    target_y.push_back(0.0F);
    target_z.push_back(target_center_z + offset_z);
  }

  auto claimed_units = claim_units(
      unit_ids, get_priority(), "expanding", context, snapshot.game_time, 2.0F);

  if (claimed_units.empty()) {
    return;
  }

  std::unordered_set<Engine::Core::EntityID> const claimed_set(claimed_units.begin(),
                                                               claimed_units.end());
  std::vector<float> filtered_x;
  std::vector<float> filtered_y;
  std::vector<float> filtered_z;

  for (size_t i = 0; i < unit_ids.size(); ++i) {
    if (claimed_set.contains(unit_ids[i])) {
      filtered_x.push_back(target_x[i]);
      filtered_y.push_back(target_y[i]);
      filtered_z.push_back(target_z[i]);
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimed_units);
  command.move_target_x = std::move(filtered_x);
  command.move_target_y = std::move(filtered_y);
  command.move_target_z = std::move(filtered_z);

  out_commands.push_back(std::move(command));
}

auto ExpandBehavior::should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }

  if (context.state != AIState::Expanding) {
    return false;
  }

  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building && enemy.spawn_type == Game::Units::SpawnType::Barracks &&
        Game::Core::is_neutral_owner(enemy.owner_id)) {
      return true;
    }
  }

  return context.has_expansion_site;
}

} // namespace Game::Systems::AI
