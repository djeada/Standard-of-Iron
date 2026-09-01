#include "defend_behavior.h"

#include <QVector3D>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../nation_registry.h"
#include "../ai_base_manager.h"
#include "../ai_formation.h"
#include "../ai_tactical.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr int k_defenders_per_threat = 2;

auto select_defended_base(const AIContext& context) -> const AIBase* {
  const AIBase* best = nullptr;

  for (const auto& base : context.bases) {
    if (!base.under_threat) {
      continue;
    }
    if (best == nullptr || base.nearby_threat_count > best->nearby_threat_count ||
        (base.nearby_threat_count == best->nearby_threat_count &&
         base.role == BaseRole::Main)) {
      best = &base;
    }
  }

  if (best != nullptr) {
    return best;
  }

  return AIBaseManager::main_base(context);
}

auto has_defensible_ground(const AIContext& context) -> bool {
  return context.primary_barracks != 0 || context.anchor_is_structural ||
         !context.bases.empty();
}

auto active_threat_count(const AIContext& context, const AIBase* defended_base) -> int {
  const int base_threats = (defended_base != nullptr)
                               ? defended_base->nearby_threat_count
                               : context.nearby_threat_count;
  return base_threats + static_cast<int>(context.buildings_under_attack.size());
}

} // namespace

void DefendBehavior::execute(const AISnapshot& snapshot,
                             AIContext& context,
                             float delta_time,
                             std::vector<AICommand>& out_commands) {
  m_defend_timer += delta_time;

  float const update_interval = context.barracks_under_threat ? 0.5F : 1.5F;

  if (m_defend_timer < update_interval) {
    return;
  }
  m_defend_timer = 0.0F;

  if (!has_defensible_ground(context)) {
    return;
  }

  const AIBase* defended_base = select_defended_base(context);

  const float defend_pos_x =
      (defended_base != nullptr) ? defended_base->center_x : context.base_pos_x;
  const float defend_pos_y = context.base_pos_y;
  const float defend_pos_z =
      (defended_base != nullptr) ? defended_base->center_z : context.base_pos_z;
  const int defended_base_id = (defended_base != nullptr) ? defended_base->id : 0;

  std::vector<const EntitySnapshot*> ready_defenders;
  std::vector<const EntitySnapshot*> engaged_defenders;
  ready_defenders.reserve(snapshot.friendly_units.size());
  engaged_defenders.reserve(snapshot.friendly_units.size());

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building) {
      continue;
    }

    if (!picks_its_own_fights(entity)) {
      continue;
    }

    if (entity.is_commander) {

      continue;
    }

    if (entity.is_assault) {
      continue;
    }

    if (is_entity_engaged(entity, snapshot.visible_enemies)) {
      engaged_defenders.push_back(&entity);
    } else {
      ready_defenders.push_back(&entity);
    }
  }

  if (ready_defenders.empty() && engaged_defenders.empty()) {
    return;
  }

  auto sort_by_distance = [&](std::vector<const EntitySnapshot*>& list) {
    std::sort(
        list.begin(),
        list.end(),
        [&](const EntitySnapshot* a, const EntitySnapshot* b) {
          float const da = distance_squared(
              a->pos_x, a->pos_y, a->pos_z, defend_pos_x, defend_pos_y, defend_pos_z);
          float const db = distance_squared(
              b->pos_x, b->pos_y, b->pos_z, defend_pos_x, defend_pos_y, defend_pos_z);
          return da < db;
        });
  };

  sort_by_distance(ready_defenders);
  sort_by_distance(engaged_defenders);

  const std::size_t total_available = ready_defenders.size() + engaged_defenders.size();

  const bool base_is_attacked = context.barracks_under_threat ||
                                context.any_base_under_threat ||
                                !context.buildings_under_attack.empty();
  const bool full_recall =
      base_is_attacked && context.strategy_config.full_recall_on_base_threat;

  const auto quiet_defenders = static_cast<std::size_t>(
      std::max(2.0F, 6.0F * context.strategy_config.defense_modifier));
  std::size_t desired_count = quiet_defenders;
  if (full_recall) {
    desired_count = total_available;
  } else if (base_is_attacked) {
    desired_count = static_cast<std::size_t>(
        std::max(1, active_threat_count(context, defended_base)) *
        k_defenders_per_threat);
  }
  desired_count = std::min(desired_count, total_available);
  const std::size_t ready_wanted =
      desired_count - std::min(desired_count, engaged_defenders.size());

  if (ready_defenders.empty() || ready_wanted == 0) {
    return;
  }

  auto preferred_ready_defenders = [&]() {
    std::vector<const EntitySnapshot*> selected;
    selected.reserve(ready_defenders.size());

    for (const auto* unit : ready_defenders) {
      if (is_reserved_unit(unit->id, context)) {
        selected.push_back(unit);
      }
    }

    const bool threat_drains_support =
        context.barracks_under_threat ||
        (context.nearby_threat_count > static_cast<int>(selected.size()));
    if (threat_drains_support) {
      for (const auto* unit : ready_defenders) {
        if (!is_reserved_unit(unit->id, context)) {
          selected.push_back(unit);
        }
      }
    }

    if (selected.empty() || full_recall) {
      selected = ready_defenders;
    }

    selected.resize(std::min(ready_wanted, selected.size()));
    return selected;
  };

  auto selected_ready_defenders = preferred_ready_defenders();
  if (selected_ready_defenders.empty()) {
    return;
  }

  if (base_is_attacked) {

    std::vector<const ContactSnapshot*> nearby_threats;
    nearby_threats.reserve(snapshot.visible_enemies.size());

    constexpr float k_base_defend_radius = 30.0F;
    const float defend_radius =
        k_base_defend_radius +
        10.0F * std::min(2.0F, context.strategy_config.defense_modifier);
    const float defend_radius_sq = defend_radius * defend_radius;

    for (const auto& enemy : snapshot.visible_enemies) {
      if (!is_war_contact(enemy)) {
        continue;
      }
      float const dist_sq = distance_squared(enemy.pos_x,
                                             enemy.pos_y,
                                             enemy.pos_z,
                                             defend_pos_x,
                                             defend_pos_y,
                                             defend_pos_z);
      if (dist_sq <= defend_radius_sq) {
        nearby_threats.push_back(&enemy);
      }
    }

    if (!nearby_threats.empty()) {

      const auto nearby_threat_units = static_cast<std::size_t>(
          std::count_if(nearby_threats.begin(),
                        nearby_threats.end(),
                        [](const ContactSnapshot* contact) {
                          return is_threatening_contact(*contact);
                        }));
      const std::size_t threat_wanted =
          nearby_threat_units * k_defenders_per_threat -
          std::min(nearby_threat_units * k_defenders_per_threat,
                   engaged_defenders.size());
      const std::size_t wanted_defenders =
          full_recall
              ? ready_defenders.size()
              : std::min(ready_defenders.size(), std::max(ready_wanted, threat_wanted));
      for (const auto* unit : ready_defenders) {
        if (selected_ready_defenders.size() >= wanted_defenders) {
          break;
        }
        if (std::find(selected_ready_defenders.begin(),
                      selected_ready_defenders.end(),
                      unit) == selected_ready_defenders.end()) {
          selected_ready_defenders.push_back(unit);
        }
      }

      auto target_info =
          TacticalUtils::select_focus_fire_target(selected_ready_defenders,
                                                  nearby_threats,
                                                  defend_pos_x,
                                                  defend_pos_y,
                                                  defend_pos_z,
                                                  context,
                                                  0);
      if (target_info.target_id != 0) {

        std::vector<Engine::Core::EntityID> defender_ids;
        defender_ids.reserve(selected_ready_defenders.size());
        for (const auto* unit : selected_ready_defenders) {
          defender_ids.push_back(unit->id);
        }

        auto claimed_units = claim_units(defender_ids,
                                         get_priority(),
                                         "defending",
                                         context,
                                         snapshot.game_time,
                                         3.0F,
                                         defended_base_id);

        if (!claimed_units.empty()) {
          AICommand attack;
          attack.type = AICommandType::AttackTarget;
          attack.units = std::move(claimed_units);
          attack.target_id = target_info.target_id;
          attack.should_chase = true;
          out_commands.push_back(std::move(attack));
          return;
        }
      }
    } else {

      const ContactSnapshot* closest_threat = nullptr;
      float closest_dist_sq = std::numeric_limits<float>::max();

      for (const auto& enemy : snapshot.visible_enemies) {
        if (!is_war_contact(enemy)) {
          continue;
        }
        float const dist_sq = distance_squared(enemy.pos_x,
                                               enemy.pos_y,
                                               enemy.pos_z,
                                               defend_pos_x,
                                               defend_pos_y,
                                               defend_pos_z);
        if (dist_sq < closest_dist_sq) {
          closest_dist_sq = dist_sq;
          closest_threat = &enemy;
        }
      }

      if ((closest_threat != nullptr) && !selected_ready_defenders.empty()) {

        std::vector<Engine::Core::EntityID> defender_ids;
        std::vector<float> target_x;
        std::vector<float> target_y;
        std::vector<float> target_z;

        for (const auto* unit : selected_ready_defenders) {
          defender_ids.push_back(unit->id);
          target_x.push_back(closest_threat->pos_x);
          target_y.push_back(closest_threat->pos_y);
          target_z.push_back(closest_threat->pos_z);
        }

        auto claimed_units = claim_units(defender_ids,
                                         get_priority(),
                                         "intercepting",
                                         context,
                                         snapshot.game_time,
                                         2.0F,
                                         defended_base_id);

        if (!claimed_units.empty()) {

          std::vector<float> filtered_x;
          std::vector<float> filtered_y;
          std::vector<float> filtered_z;
          {
            const std::unordered_set<Engine::Core::EntityID> claimed_set(
                claimed_units.begin(), claimed_units.end());
            for (size_t i = 0; i < defender_ids.size(); ++i) {
              if (static_cast<unsigned int>(claimed_set.contains(defender_ids[i])) !=
                  0U) {
                filtered_x.push_back(target_x[i]);
                filtered_y.push_back(target_y[i]);
                filtered_z.push_back(target_z[i]);
              }
            }
          }

          AICommand move;
          move.type = AICommandType::MoveUnits;
          move.units = std::move(claimed_units);
          move.move_target_x = std::move(filtered_x);
          move.move_target_y = std::move(filtered_y);
          move.move_target_z = std::move(filtered_z);
          out_commands.push_back(std::move(move));
          return;
        }
      }
    }
  }

  std::vector<const EntitySnapshot*> unclaimed_defenders;
  for (const auto* unit : selected_ready_defenders) {
    auto it = context.assigned_units.find(unit->id);
    if (it == context.assigned_units.end()) {
      unclaimed_defenders.push_back(unit);
    }
  }

  if (unclaimed_defenders.empty()) {
    return;
  }

  QVector3D const defend_pos(defend_pos_x, defend_pos_y, defend_pos_z);
  AIFormationRequest formation_request;
  formation_request.player_id = context.player_id;
  formation_request.nation = context.nation;
  formation_request.anchor = defend_pos;
  formation_request.spacing = 3.0F;
  formation_request.intent = Game::Formation::ArmyFormationIntent::Defensive;
  auto targets = plan_ai_formation(formation_request, unclaimed_defenders);

  std::vector<Engine::Core::EntityID> units_to_move;
  std::vector<float> target_x;
  std::vector<float> target_y;
  std::vector<float> target_z;
  units_to_move.reserve(unclaimed_defenders.size());
  target_x.reserve(unclaimed_defenders.size());
  target_y.reserve(unclaimed_defenders.size());
  target_z.reserve(unclaimed_defenders.size());

  for (size_t i = 0; i < unclaimed_defenders.size(); ++i) {
    const auto* entity = unclaimed_defenders[i];
    const auto& target = targets[i];

    float const dx = entity->pos_x - target.x();
    float const dz = entity->pos_z - target.z();
    float const distance_sq = dx * dx + dz * dz;

    if (distance_sq < 1.0F * 1.0F) {
      continue;
    }

    units_to_move.push_back(entity->id);
    target_x.push_back(target.x());
    target_y.push_back(target.y());
    target_z.push_back(target.z());
  }

  if (units_to_move.empty()) {
    return;
  }

  auto claimed_for_move = claim_units(units_to_move,
                                      BehaviorPriority::Low,
                                      "positioning",
                                      context,
                                      snapshot.game_time,
                                      1.5F,
                                      defended_base_id);

  if (claimed_for_move.empty()) {
    return;
  }

  std::vector<float> filtered_x;
  std::vector<float> filtered_y;
  std::vector<float> filtered_z;
  {
    const std::unordered_set<Engine::Core::EntityID> claimed_set(
        claimed_for_move.begin(), claimed_for_move.end());
    for (size_t i = 0; i < units_to_move.size(); ++i) {
      if (static_cast<unsigned int>(claimed_set.contains(units_to_move[i])) != 0U) {
        filtered_x.push_back(target_x[i]);
        filtered_y.push_back(target_y[i]);
        filtered_z.push_back(target_z[i]);
      }
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimed_for_move);
  command.move_target_x = std::move(filtered_x);
  command.move_target_y = std::move(filtered_y);
  command.move_target_z = std::move(filtered_z);
  out_commands.push_back(std::move(command));
}

auto DefendBehavior::should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool {
  (void)snapshot;

  if (!has_defensible_ground(context)) {
    return false;
  }

  if (context.barracks_under_threat || !context.buildings_under_attack.empty()) {
    return true;
  }

  if (context.state == AIState::Defending && context.idle_units > 0) {
    return true;
  }

  if (context.average_health < 0.6F && context.total_units > 0) {
    return true;
  }

  return false;
}

} // namespace Game::Systems::AI
