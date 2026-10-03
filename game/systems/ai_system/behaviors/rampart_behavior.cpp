#include "rampart_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../../../units/spawn_type.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_threat_reach = 90.0F;
constexpr float k_quiet_front_distance = 40.0F;

auto is_wall_archer(const EntitySnapshot& unit) -> bool {
  return !unit.is_building && !unit.is_commander && !unit.is_assault &&
         unit.spawn_type == Game::Units::SpawnType::Archer;
}

struct Front {
  float x = 0.0F;
  float z = 0.0F;
};

auto choose_front(const AISnapshot& snapshot, const AIContext& context) -> Front {
  const ContactSnapshot* nearest = nullptr;
  float best = k_threat_reach * k_threat_reach;
  for (const auto& enemy : snapshot.visible_enemies) {
    if (!is_threatening_contact(enemy) || enemy.is_building) {
      continue;
    }
    const float dist_sq = distance_squared(
        enemy.pos_x, 0.0F, enemy.pos_z, context.base_pos_x, 0.0F, context.base_pos_z);
    if (dist_sq < best) {
      best = dist_sq;
      nearest = &enemy;
    }
  }
  if (nearest != nullptr) {
    return {nearest->pos_x, nearest->pos_z};
  }
  return {context.base_pos_x + (context.settlement_facing_x * k_quiet_front_distance),
          context.base_pos_z + (context.settlement_facing_z * k_quiet_front_distance)};
}

auto is_free_post(const WallPostSnapshot& post,
                  const std::vector<WallPostSnapshot>& taken) -> bool {
  return std::none_of(
      taken.begin(), taken.end(), [&post](const WallPostSnapshot& other) {
        return distance_squared(
                   post.pos_x, 0.0F, post.pos_z, other.pos_x, 0.0F, other.pos_z) <
               RampartBehavior::k_post_spacing * RampartBehavior::k_post_spacing;
      });
}

} // namespace

void RampartBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_decision_seconds) {
    return;
  }
  m_timer = 0.0F;

  std::erase_if(m_postings, [&snapshot](const auto& entry) {
    return std::none_of(snapshot.friendly_units.begin(),
                        snapshot.friendly_units.end(),
                        [&entry](const EntitySnapshot& unit) {
                          return unit.id == entry.first && unit.health > 0;
                        });
  });

  std::erase_if(m_shunned, [&snapshot](const ShunnedPost& post) {
    return post.until <= snapshot.game_time;
  });
  for (auto it = m_postings.begin(); it != m_postings.end();) {
    if (snapshot.game_time - it->second.since > k_climb_patience_seconds) {
      m_shunned.push_back({it->second.x,
                           it->second.z,
                           snapshot.game_time + k_unreachable_post_seconds});
      release_units({it->first}, context);
      it = m_postings.erase(it);
    } else {
      ++it;
    }
  }
  std::vector<WallPostSnapshot> taken = snapshot.wall_garrison;
  for (const auto& [id, posting] : m_postings) {
    taken.push_back({posting.x, posting.z});
  }

  for (const auto& post : m_shunned) {
    taken.push_back({post.x, post.z});
  }

  std::vector<const EntitySnapshot*> idle_archers;
  for (const auto& unit : snapshot.friendly_units) {
    if (!is_wall_archer(unit) || unit.on_wall_stair || unit.fighting_troops ||
        m_postings.contains(unit.id)) {
      continue;
    }
    const auto assigned = context.assigned_units.find(unit.id);
    if (assigned != context.assigned_units.end() &&
        assigned->second.owner_priority > get_priority()) {
      continue;
    }
    idle_archers.push_back(&unit);
  }

  if (idle_archers.empty()) {
    return;
  }

  const Front front = choose_front(snapshot, context);
  std::vector<const WallPostSnapshot*> posts;
  posts.reserve(snapshot.wall_posts.size());
  for (const auto& post : snapshot.wall_posts) {
    posts.push_back(&post);
  }
  std::sort(
      posts.begin(),
      posts.end(),
      [&front](const WallPostSnapshot* lhs, const WallPostSnapshot* rhs) {
        return distance_squared(lhs->pos_x, 0.0F, lhs->pos_z, front.x, 0.0F, front.z) <
               distance_squared(rhs->pos_x, 0.0F, rhs->pos_z, front.x, 0.0F, front.z);
      });

  AICommand move;
  move.type = AICommandType::MoveUnits;
  move.owner = get_priority();
  std::vector<Engine::Core::EntityID> claimed_ids;

  for (const auto* post : posts) {
    if (idle_archers.empty()) {
      break;
    }
    if (!is_free_post(*post, taken)) {
      continue;
    }
    auto nearest = std::min_element(
        idle_archers.begin(),
        idle_archers.end(),
        [post](const EntitySnapshot* lhs, const EntitySnapshot* rhs) {
          return distance_squared(
                     lhs->pos_x, 0.0F, lhs->pos_z, post->pos_x, 0.0F, post->pos_z) <
                 distance_squared(
                     rhs->pos_x, 0.0F, rhs->pos_z, post->pos_x, 0.0F, post->pos_z);
        });
    const EntitySnapshot* archer = *nearest;
    idle_archers.erase(nearest);

    const auto claimed = claim_units({archer->id},
                                     get_priority(),
                                     k_task_name,
                                     context,
                                     snapshot.game_time,
                                     k_decision_seconds);
    if (claimed.empty()) {
      continue;
    }
    move.units.push_back(archer->id);
    move.move_target_x.push_back(post->pos_x);
    move.move_target_y.push_back(0.0F);
    move.move_target_z.push_back(post->pos_z);
    m_postings[archer->id] = {post->pos_x, post->pos_z, snapshot.game_time};
    taken.push_back(*post);
  }

  if (!move.units.empty()) {
    out_commands.push_back(std::move(move));
  }
}

auto RampartBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  if (context.strategy_config.posture != AIPosture::Garrison) {
    return false;
  }
  return !snapshot.wall_posts.empty();
}

} // namespace Game::Systems::AI
