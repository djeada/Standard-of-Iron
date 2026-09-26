#include "gold_vein_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../../../units/spawn_type.h"
#include "../../../units/troop_type.h"
#include "../../nation_registry.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_decision_seconds = 2.0F;
constexpr std::size_t k_party_size = 2;
constexpr float k_claim_patience_seconds = 180.0F;
constexpr float k_give_up_seconds = 300.0F;
constexpr float k_approach_metres = 4.0F;

auto find_vein(const AISnapshot& snapshot,
               Engine::Core::EntityID anchor) -> const GoldVeinSnapshot* {
  for (const auto& vein : snapshot.gold_veins) {
    if (vein.anchor_id == anchor) {
      return &vein;
    }
  }
  return nullptr;
}

auto is_commander(const EntitySnapshot& unit) -> bool {
  const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
  return troop.has_value() && Game::Units::is_commander_troop(*troop);
}

} // namespace

void GoldVeinBehavior::execute(const AISnapshot& snapshot,
                               AIContext& context,
                               float delta_time,
                               std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_decision_seconds) {
    return;
  }
  m_timer = 0.0F;

  std::erase_if(m_party, [&snapshot](Engine::Core::EntityID id) {
    return std::none_of(
        snapshot.friendly_units.begin(),
        snapshot.friendly_units.end(),
        [id](const EntitySnapshot& unit) { return unit.id == id && unit.health > 0; });
  });

  if (!m_party.empty()) {
    const auto* vein = find_vein(snapshot, m_vein);
    const bool won = vein != nullptr && vein->owner_id == context.player_id;
    const bool stale = snapshot.game_time - m_sent_at > k_claim_patience_seconds;
    if (vein == nullptr || won || stale) {
      if (stale && !won) {
        m_given_up = m_vein;
        m_given_up_until = snapshot.game_time + k_give_up_seconds;
      }
      release_units(m_party, context);
      m_party.clear();
      m_vein = 0;
      return;
    }
    claim_units(m_party,
                get_priority(),
                "claiming_vein",
                context,
                snapshot.game_time,
                k_decision_seconds);
    return;
  }

  const GoldVeinSnapshot* target = nullptr;
  float best = std::numeric_limits<float>::infinity();
  for (const auto& vein : snapshot.gold_veins) {
    if (vein.owner_id == context.player_id ||
        (vein.anchor_id == m_given_up && snapshot.game_time < m_given_up_until)) {
      continue;
    }
    const float dist_sq = distance_squared(
        vein.pos_x, 0.0F, vein.pos_z, context.base_pos_x, 0.0F, context.base_pos_z);
    if (dist_sq < best) {
      best = dist_sq;
      target = &vein;
    }
  }
  if (target == nullptr) {
    return;
  }

  std::vector<const EntitySnapshot*> candidates;
  for (const auto* unit : collect_attack_force_units(snapshot, context)) {
    if (marches_with_the_wave(unit->id, context) || is_commander(*unit)) {
      continue;
    }
    if (std::find(context.garrison_unit_ids.begin(),
                  context.garrison_unit_ids.end(),
                  unit->id) != context.garrison_unit_ids.end()) {
      continue;
    }
    candidates.push_back(unit);
  }
  if (candidates.size() < k_party_size) {
    return;
  }
  std::sort(
      candidates.begin(),
      candidates.end(),
      [target](const EntitySnapshot* lhs, const EntitySnapshot* rhs) {
        return distance_squared(
                   lhs->pos_x, 0.0F, lhs->pos_z, target->pos_x, 0.0F, target->pos_z) <
               distance_squared(
                   rhs->pos_x, 0.0F, rhs->pos_z, target->pos_x, 0.0F, target->pos_z);
      });
  std::vector<Engine::Core::EntityID> wanted;
  for (std::size_t i = 0; i < k_party_size; ++i) {
    wanted.push_back(candidates[i]->id);
  }
  auto claimed = claim_units(
      wanted, get_priority(), "claiming_vein", context, snapshot.game_time, 2.0F);
  if (claimed.size() < k_party_size) {
    release_units(claimed, context);
    return;
  }

  float from_x = context.base_pos_x - target->pos_x;
  float from_z = context.base_pos_z - target->pos_z;
  const float length = std::sqrt((from_x * from_x) + (from_z * from_z));
  if (length > 0.1F) {
    from_x /= length;
    from_z /= length;
  }
  AICommand move;
  move.type = AICommandType::MoveUnits;
  move.owner = get_priority();
  for (std::size_t i = 0; i < claimed.size(); ++i) {
    const float side = i == 0 ? 1.5F : -1.5F;
    move.units.push_back(claimed[i]);
    move.move_target_x.push_back(target->pos_x + (from_x * k_approach_metres) -
                                 (from_z * side));
    move.move_target_y.push_back(0.0F);
    move.move_target_z.push_back(target->pos_z + (from_z * k_approach_metres) +
                                 (from_x * side));
  }
  out_commands.push_back(std::move(move));

  m_party = std::move(claimed);
  m_vein = target->anchor_id;
  m_sent_at = snapshot.game_time;
}

auto GoldVeinBehavior::should_execute(const AISnapshot& snapshot,
                                      const AIContext& context) const -> bool {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }
  return context.has_base_anchor && (!snapshot.gold_veins.empty() || !m_party.empty());
}

} // namespace Game::Systems::AI
