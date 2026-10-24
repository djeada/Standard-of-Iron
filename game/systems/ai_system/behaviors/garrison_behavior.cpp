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

auto is_engine(const ContactSnapshot& contact) -> bool {
  return contact.spawn_type == Game::Units::SpawnType::Ram ||
         contact.spawn_type == Game::Units::SpawnType::SiegeTower;
}

auto within_sally_reach(const AISnapshot& snapshot, float x, float z) -> bool {
  return snapshot.ward.contains(x, z, Ward::k_apron + GarrisonBehavior::k_sally_reach);
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

    m_posts.try_emplace(unit.id, Post{unit.pos_x, unit.pos_z});
  }
  std::erase_if(m_posts,
                [&alive](const auto& entry) { return !alive.contains(entry.first); });
  std::erase_if(m_breach_slots,
                [&alive](const auto& entry) { return !alive.contains(entry.first); });
  std::erase_if(m_sally_party,
                [&alive](Engine::Core::EntityID id) { return !alive.contains(id); });
  if (infantry.empty()) {
    return;
  }

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
  update_breach(snapshot, infantry);
  sally(snapshot, infantry, out_commands);
  hold_posts(snapshot, infantry, context, out_commands);
}

auto GarrisonBehavior::find_breach(const AISnapshot& snapshot)
    -> std::optional<Breach> {
  std::vector<const ContactSnapshot*> attackers;
  for (const auto& enemy : snapshot.visible_enemies) {
    bool const ladder = enemy.spawn_type == Game::Units::SpawnType::WallLadder;
    if ((enemy.is_building && !ladder) || !is_war_contact(enemy) ||
        !snapshot.ward.within_apron(enemy.pos_x, enemy.pos_z)) {
      continue;
    }
    attackers.push_back(&enemy);
  }
  if (attackers.empty()) {
    return std::nullopt;
  }

  auto const outranks = [](const Breach& a, const Breach& b) {
    if ((a.engines > 0) != (b.engines > 0)) {
      return a.engines > 0;
    }
    return a.pressure > b.pressure;
  };
  std::optional<Breach> worst;
  float const watch_sq = k_breach_watch * k_breach_watch;
  for (const auto& piece : snapshot.friendly_units) {
    if (!piece.is_building || piece.health <= 0 ||
        !Game::Units::is_wall_network_spawn(piece.spawn_type)) {
      continue;
    }
    Breach candidate{.piece = piece.id, .x = piece.pos_x, .z = piece.pos_z};
    int counted = 0;
    for (const auto* enemy : attackers) {
      if (planar_distance_sq(enemy->pos_x, enemy->pos_z, piece.pos_x, piece.pos_z) >
          watch_sq) {
        continue;
      }
      bool const breaker =
          is_engine(*enemy) || enemy->spawn_type == Game::Units::SpawnType::WallLadder;
      candidate.pressure += breaker ? k_engine_pressure : 1.0F;
      candidate.engines += breaker ? 1 : 0;
      candidate.attackers_x += enemy->pos_x;
      candidate.attackers_z += enemy->pos_z;
      ++counted;
    }
    if (counted == 0) {
      continue;
    }
    candidate.attackers_x /= static_cast<float>(counted);
    candidate.attackers_z /= static_cast<float>(counted);

    float const needed =
        candidate.engines > 0 ? k_breach_pressure : 2.0F * k_breach_pressure;
    if (candidate.pressure < needed) {
      continue;
    }
    if (!worst.has_value() || outranks(candidate, *worst)) {
      worst = candidate;
    }
  }
  return worst;
}

void GarrisonBehavior::update_breach(
    const AISnapshot& snapshot, const std::vector<const EntitySnapshot*>& infantry) {
  auto const found = find_breach(snapshot);
  bool reform = false;
  if (found.has_value()) {

    bool const same_front =
        m_breach.has_value() &&
        planar_distance_sq(found->x, found->z, m_breach->x, m_breach->z) <=
            k_breach_watch * k_breach_watch;
    if (same_front) {
      m_breach->pressure = found->pressure;
      m_breach->engines = found->engines;
      m_breach->attackers_x = found->attackers_x;
      m_breach->attackers_z = found->attackers_z;
      m_breach_last_pressed = snapshot.game_time;
    } else if (!m_breach.has_value() ||
               (found->engines >= m_breach->engines &&
                found->pressure >= m_breach->pressure * k_breach_switch_ratio)) {
      m_breach = found;
      m_breach_last_pressed = snapshot.game_time;
      reform = true;
    }
  }
  if (m_breach.has_value() &&
      snapshot.game_time - m_breach_last_pressed > k_breach_linger_seconds) {
    m_breach.reset();
    m_breach_slots.clear();
  }
  if (!m_breach.has_value()) {
    return;
  }

  std::vector<const EntitySnapshot*> muster;
  for (const auto* unit : infantry) {
    if (std::find(m_sally_party.begin(), m_sally_party.end(), unit->id) ==
        m_sally_party.end()) {
      muster.push_back(unit);
    }
  }
  bool const new_faces =
      std::any_of(muster.begin(), muster.end(), [this](const EntitySnapshot* unit) {
        return !m_breach_slots.contains(unit->id);
      });
  if (!reform && !new_faces) {
    return;
  }

  float in_x = m_breach->x - m_breach->attackers_x;
  float in_z = m_breach->z - m_breach->attackers_z;
  if (std::hypot(in_x, in_z) < 0.5F) {
    in_x = snapshot.ward.x - m_breach->x;
    in_z = snapshot.ward.z - m_breach->z;
  }

  if (std::abs(in_x) >= std::abs(in_z)) {
    in_x = in_x >= 0.0F ? 1.0F : -1.0F;
    in_z = 0.0F;
  } else {
    in_x = 0.0F;
    in_z = in_z >= 0.0F ? 1.0F : -1.0F;
  }
  float muster_x = m_breach->x + (in_x * k_breach_muster_depth);
  float muster_z = m_breach->z + (in_z * k_breach_muster_depth);
  if (!snapshot.ward.inside(muster_x, muster_z)) {
    muster_x = m_breach->x + ((snapshot.ward.x - m_breach->x) * 0.25F);
    muster_z = m_breach->z + ((snapshot.ward.z - m_breach->z) * 0.25F);
  }

  std::sort(muster.begin(), muster.end(), [this](const auto* lhs, const auto* rhs) {
    float const dl =
        planar_distance_sq(lhs->pos_x, lhs->pos_z, m_breach->x, m_breach->z);
    float const dr =
        planar_distance_sq(rhs->pos_x, rhs->pos_z, m_breach->x, m_breach->z);
    return dl != dr ? dl < dr : lhs->id < rhs->id;
  });
  float const across_x = -in_z;
  float const across_z = in_x;
  m_breach_slots.clear();
  for (std::size_t i = 0; i < muster.size(); ++i) {
    auto const rank = static_cast<float>(i / k_breach_files);
    auto const file = static_cast<float>(i % k_breach_files) -
                      (static_cast<float>(k_breach_files - 1) * 0.5F);
    float const x = muster_x + (in_x * rank * k_breach_rank_depth) +
                    (across_x * file * k_breach_file_width);
    float const z = muster_z + (in_z * rank * k_breach_rank_depth) +
                    (across_z * file * k_breach_file_width);
    m_breach_slots[muster[i]->id] = {x, z};
  }
}

auto GarrisonBehavior::post_of(Engine::Core::EntityID unit) const -> const Post* {
  if (auto const slot = m_breach_slots.find(unit); slot != m_breach_slots.end()) {
    return &slot->second;
  }
  auto const home = m_posts.find(unit);
  return home != m_posts.end() ? &home->second : nullptr;
}

auto GarrisonBehavior::counterattack(const AISnapshot& snapshot,
                                     const std::vector<const EntitySnapshot*>& infantry,
                                     std::vector<AICommand>& out_commands) -> bool {

  std::vector<const ContactSnapshot*> intruders;
  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building || !is_war_contact(enemy) || is_engine(enemy)) {
      continue;
    }
    if (snapshot.ward.inside(enemy.pos_x, enemy.pos_z)) {
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
            enemy.pos_x, enemy.pos_z, snapshot.ward.x, snapshot.ward.z);
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

    bool const under_way = m_sally_target == engine->id && !m_sally_party.empty();
    int const tolerated =
        under_way ? static_cast<int>(m_sally_party.size()) : k_lonely_escort;
    bool const lonely = escort <= tolerated;
    if (!lonely) {
      if (!m_sally_party.empty()) {

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
    const Post* post = post_of(unit->id);
    if (post == nullptr) {
      continue;
    }
    float const off =
        std::sqrt(planar_distance_sq(unit->pos_x, unit->pos_z, post->x, post->z));
    bool const heading_home =
        unit->movement.has_target && unit->movement.has_objective &&
        std::sqrt(planar_distance_sq(
            unit->movement.objective_x, unit->movement.objective_z, post->x, post->z)) <
            k_post_slack;
    if (off <= k_post_slack || heading_home) {
      continue;
    }

    if (unit->attack_target_id != 0) {

      bool const target_within_reach =
          std::any_of(snapshot.visible_enemies.begin(),
                      snapshot.visible_enemies.end(),
                      [unit, &snapshot](const ContactSnapshot& enemy) {
                        return enemy.id == unit->attack_target_id &&
                               snapshot.ward.within_reach(enemy.pos_x, enemy.pos_z);
                      });
      if (target_within_reach) {
        continue;
      }
    }
    move.units.push_back(unit->id);
    move.move_target_x.push_back(post->x);
    move.move_target_y.push_back(0.0F);
    move.move_target_z.push_back(post->z);
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
