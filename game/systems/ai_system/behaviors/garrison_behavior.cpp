#include "garrison_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../../units/spawn_type.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_inside_margin = 0.5F;

auto is_engine(const ContactSnapshot& contact) -> bool {
  return contact.spawn_type == Game::Units::SpawnType::Ram ||
         contact.spawn_type == Game::Units::SpawnType::SiegeTower;
}

auto inside_walls(const AISnapshot& snapshot, float x, float z) -> bool {
  float const half_x =
      snapshot.ward_half_x - GarrisonBehavior::k_ward_apron - k_inside_margin;
  float const half_z =
      snapshot.ward_half_z - GarrisonBehavior::k_ward_apron - k_inside_margin;
  return std::abs(x - snapshot.ward_x) < half_x &&
         std::abs(z - snapshot.ward_z) < half_z;
}

auto within_sally_reach(const AISnapshot& snapshot, float x, float z) -> bool {
  float const half_x = snapshot.ward_half_x + GarrisonBehavior::k_sally_reach;
  float const half_z = snapshot.ward_half_z + GarrisonBehavior::k_sally_reach;
  return std::abs(x - snapshot.ward_x) < half_x &&
         std::abs(z - snapshot.ward_z) < half_z;
}

auto planar_distance_sq(float ax, float az, float bx, float bz) -> float {
  return ((ax - bx) * (ax - bx)) + ((az - bz) * (az - bz));
}

void attack_with(std::vector<Engine::Core::EntityID> units,
                 Engine::Core::EntityID target,
                 BehaviorPriority owner,
                 std::vector<AICommand>& out_commands,
                 bool sally = false) {
  if (units.empty()) {
    return;
  }
  AICommand attack;
  attack.type = AICommandType::AttackTarget;
  attack.owner = owner;
  attack.units = std::move(units);
  attack.target_id = target;
  attack.should_chase = true;
  attack.sally = sally;
  out_commands.push_back(std::move(attack));
}

} // namespace

auto GarrisonBehavior::is_garrison_infantry(const EntitySnapshot& unit) -> bool {
  if (unit.is_building || unit.is_commander || unit.is_assault || unit.on_wall_stair ||
      unit.health <= 0 || !picks_its_own_fights(unit)) {
    return false;
  }
  switch (unit.spawn_type) {
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
  case Game::Units::SpawnType::Ram:
  case Game::Units::SpawnType::SiegeTower:
    return false;
  default:
    return true;
  }
}

void GarrisonBehavior::execute(const AISnapshot& snapshot,
                               AIContext& context,
                               float delta_time,
                               std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_decision_seconds) {
    return;
  }
  m_timer = 0.0F;

  std::vector<const EntitySnapshot*> infantry;
  std::unordered_set<Engine::Core::EntityID> alive;
  for (const auto& unit : snapshot.friendly_units) {
    if (!is_garrison_infantry(unit)) {
      continue;
    }
    infantry.push_back(&unit);
    alive.insert(unit.id);
    // A company's post is wherever it first stood: the map's authored post, or
    // the ground a reinforcement was mustered on.
    m_posts.try_emplace(unit.id, Post{unit.pos_x, unit.pos_z});
  }
  std::erase_if(m_posts,
                [&alive](const auto& entry) { return !alive.contains(entry.first); });
  std::erase_if(m_sally_party,
                [&alive](Engine::Core::EntityID id) { return !alive.contains(id); });
  if (infantry.empty()) {
    return;
  }

  // Hold every company for this behavior so the general defence, the muster
  // and the retreat never march the town's garrison off somewhere else.
  {
    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(infantry.size());
    for (const auto* unit : infantry) {
      ids.push_back(unit->id);
    }
    (void)claim_units(ids,
                      get_priority(),
                      k_task_name,
                      context,
                      snapshot.game_time,
                      k_decision_seconds * 3.0F);
  }

  if (counterattack(snapshot, infantry, out_commands)) {
    return;
  }
  sally(snapshot, infantry, out_commands);
  hold_posts(snapshot, infantry, context, out_commands);
}

auto GarrisonBehavior::counterattack(const AISnapshot& snapshot,
                                     const std::vector<const EntitySnapshot*>& infantry,
                                     std::vector<AICommand>& out_commands) -> bool {
  // The enemy inside the walls: every company turns on the nearest of them.
  std::vector<const ContactSnapshot*> intruders;
  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building || !is_war_contact(enemy) || is_engine(enemy)) {
      continue;
    }
    if (inside_walls(snapshot, enemy.pos_x, enemy.pos_z)) {
      intruders.push_back(&enemy);
    }
  }
  if (!intruders.empty()) {
    m_sally_target = 0;
    m_sally_party.clear();
    std::unordered_map<Engine::Core::EntityID, std::vector<Engine::Core::EntityID>>
        orders;
    for (const auto* unit : infantry) {
      const ContactSnapshot* nearest = nullptr;
      float best = std::numeric_limits<float>::max();
      for (const auto* enemy : intruders) {
        float const d =
            planar_distance_sq(unit->pos_x, unit->pos_z, enemy->pos_x, enemy->pos_z);
        if (d < best) {
          best = d;
          nearest = enemy;
        }
      }
      if (nearest == nullptr) {
        continue;
      }
      // Already locked with someone inside: let him finish that fight.
      bool const fighting_inside =
          unit->attack_target_id != 0 &&
          std::any_of(intruders.begin(), intruders.end(), [unit](const auto* enemy) {
            return enemy->id == unit->attack_target_id;
          });
      if (fighting_inside) {
        continue;
      }
      orders[nearest->id].push_back(unit->id);
    }
    for (auto& [target, units] : orders) {
      attack_with(std::move(units), target, get_priority(), out_commands);
    }
    return true;
  }
  return false;
}

void GarrisonBehavior::sally(const AISnapshot& snapshot,
                             const std::vector<const EntitySnapshot*>& infantry,
                             std::vector<AICommand>& out_commands) {
  // A siege engine come up to the walls with no escort to speak of: a small
  // party goes out, burns it and comes home.
  const ContactSnapshot* engine = nullptr;
  if (m_sally_target != 0) {
    for (const auto& enemy : snapshot.visible_enemies) {
      if (enemy.id == m_sally_target) {
        engine = &enemy;
        break;
      }
    }
  }
  if (engine == nullptr) {
    m_sally_target = 0;
    m_sally_party.clear();
    if (snapshot.game_time >= m_sally_cooldown_until) {
      float best = std::numeric_limits<float>::max();
      for (const auto& enemy : snapshot.visible_enemies) {
        if (!is_engine(enemy) ||
            !within_sally_reach(snapshot, enemy.pos_x, enemy.pos_z)) {
          continue;
        }
        float const d = planar_distance_sq(
            enemy.pos_x, enemy.pos_z, snapshot.ward_x, snapshot.ward_z);
        if (d < best) {
          best = d;
          engine = &enemy;
        }
      }
    }
  }
  if (engine != nullptr) {
    int escort = 0;
    for (const auto& enemy : snapshot.visible_enemies) {
      if (enemy.is_building || is_engine(enemy) || !is_war_contact(enemy)) {
        continue;
      }
      if (planar_distance_sq(enemy.pos_x, enemy.pos_z, engine->pos_x, engine->pos_z) <=
          k_escort_radius * k_escort_radius) {
        ++escort;
      }
    }
    bool const lonely = escort <= k_lonely_escort;
    if (!lonely) {
      if (!m_sally_party.empty()) {
        // The escort has come up: the party is called back before it is cut off.
        m_sally_cooldown_until = snapshot.game_time + k_sally_cooldown_seconds;
      }
      m_sally_target = 0;
      m_sally_party.clear();
    } else {
      if (m_sally_target != engine->id) {
        m_sally_target = engine->id;
        m_sally_party.clear();
      }
      if (static_cast<int>(m_sally_party.size()) < k_sally_party) {
        std::vector<const EntitySnapshot*> free;
        for (const auto* unit : infantry) {
          if (std::find(m_sally_party.begin(), m_sally_party.end(), unit->id) ==
                  m_sally_party.end() &&
              unit->attack_target_id == 0 && !unit->engaged) {
            free.push_back(unit);
          }
        }
        std::sort(free.begin(), free.end(), [engine](const auto* a, const auto* b) {
          return planar_distance_sq(a->pos_x, a->pos_z, engine->pos_x, engine->pos_z) <
                 planar_distance_sq(b->pos_x, b->pos_z, engine->pos_x, engine->pos_z);
        });
        for (const auto* unit : free) {
          if (static_cast<int>(m_sally_party.size()) >= k_sally_party) {
            break;
          }
          m_sally_party.push_back(unit->id);
        }
      }
      std::vector<Engine::Core::EntityID> to_order;
      for (const auto* unit : infantry) {
        if (unit->attack_target_id != engine->id &&
            std::find(m_sally_party.begin(), m_sally_party.end(), unit->id) !=
                m_sally_party.end()) {
          to_order.push_back(unit->id);
        }
      }
      attack_with(std::move(to_order), engine->id, get_priority(), out_commands, true);
    }
  }
}

void GarrisonBehavior::hold_posts(const AISnapshot& snapshot,
                                  const std::vector<const EntitySnapshot*>& infantry,
                                  AIContext& context,
                                  std::vector<AICommand>& out_commands) {
  (void)snapshot;
  (void)context;
  AICommand move;
  move.type = AICommandType::MoveUnits;
  move.owner = get_priority();
  for (const auto* unit : infantry) {
    if (std::find(m_sally_party.begin(), m_sally_party.end(), unit->id) !=
            m_sally_party.end() ||
        unit->engaged) {
      continue;
    }
    auto const post = m_posts.find(unit->id);
    if (post == m_posts.end()) {
      continue;
    }
    float const off = std::sqrt(
        planar_distance_sq(unit->pos_x, unit->pos_z, post->second.x, post->second.z));
    bool const heading_home =
        unit->movement.has_target && unit->movement.has_objective &&
        std::sqrt(planar_distance_sq(unit->movement.objective_x,
                                     unit->movement.objective_z,
                                     post->second.x,
                                     post->second.z)) < k_post_slack;
    if (off <= k_post_slack || heading_home) {
      continue;
    }
    // Chasing an enemy within the leash is the leash's business; anything
    // else - an engine burnt, an intruder dead - sends the company home.
    if (unit->attack_target_id != 0) {
      // A fight inside the leash is his to finish; a target beyond it (an
      // alert pointed him at an engine on the ramp) only strands him at the
      // edge of the ward.
      bool const target_within_reach =
          std::any_of(snapshot.visible_enemies.begin(),
                      snapshot.visible_enemies.end(),
                      [unit, &snapshot](const ContactSnapshot& enemy) {
                        return enemy.id == unit->attack_target_id &&
                               std::abs(enemy.pos_x - snapshot.ward_x) <
                                   inner_half(snapshot.ward_half_x) &&
                               std::abs(enemy.pos_z - snapshot.ward_z) <
                                   inner_half(snapshot.ward_half_z);
                      });
      if (target_within_reach) {
        continue;
      }
    }
    move.units.push_back(unit->id);
    move.move_target_x.push_back(post->second.x);
    move.move_target_y.push_back(0.0F);
    move.move_target_z.push_back(post->second.z);
  }
  if (!move.units.empty()) {
    out_commands.push_back(std::move(move));
  }
}

auto GarrisonBehavior::should_execute(const AISnapshot& snapshot,
                                      const AIContext& context) const -> bool {
  return context.strategy_config.posture == AIPosture::Garrison && snapshot.has_ward;
}

} // namespace Game::Systems::AI
