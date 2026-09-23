#include "ally_aid_behavior.h"

#include <QDebug>
#include <QtGlobal>

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_aid_interval_seconds = 3.0F;
constexpr float k_aid_share = 0.5F;
constexpr int k_aid_minimum = 2;

auto nearest_call(const AISnapshot& snapshot,
                  const AIContext& context) -> const AllyCall* {
  const AllyCall* best = nullptr;
  float best_sq = k_ally_aid_reach * k_ally_aid_reach;
  for (const auto& call : snapshot.allies_under_attack) {
    const float sq = distance_squared(
        call.pos_x, 0.0F, call.pos_z, context.base_pos_x, 0.0F, context.base_pos_z);
    if (sq <= best_sq) {
      best_sq = sq;
      best = &call;
    }
  }
  return best;
}

} // namespace

auto AllyAidBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  return !snapshot.allies_under_attack.empty() && context.has_base_anchor &&
         !context.barracks_under_threat && context.state != AIState::Defending &&
         context.state != AIState::Retreating;
}

void AllyAidBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_aid_interval_seconds) {
    return;
  }
  m_timer = 0.0F;

  const AllyCall* call = nearest_call(snapshot, context);
  if (call == nullptr) {
    return;
  }

  const std::unordered_set<Engine::Core::EntityID> wave(context.wave.members.begin(),
                                                        context.wave.members.end());
  const std::unordered_set<Engine::Core::EntityID> garrison(
      context.garrison_unit_ids.begin(), context.garrison_unit_ids.end());
  std::vector<const EntitySnapshot*> spare;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.is_commander || entity.health <= 0 ||
        entity.engaged || !marches_with_the_army(entity) || wave.contains(entity.id) ||
        garrison.contains(entity.id)) {
      continue;
    }
    spare.push_back(&entity);
  }
  const int sent = std::max(
      k_aid_minimum, static_cast<int>(static_cast<float>(spare.size()) * k_aid_share));
  if (static_cast<int>(spare.size()) < k_aid_minimum) {
    return;
  }
  std::sort(spare.begin(), spare.end(), [&](const auto* a, const auto* b) {
    return distance_squared(a->pos_x, 0.0F, a->pos_z, call->pos_x, 0.0F, call->pos_z) <
           distance_squared(b->pos_x, 0.0F, b->pos_z, call->pos_x, 0.0F, call->pos_z);
  });
  std::vector<Engine::Core::EntityID> ids;
  for (int i = 0; i < sent && i < static_cast<int>(spare.size()); ++i) {
    ids.push_back(spare[static_cast<std::size_t>(i)]->id);
  }
  auto claimed = claim_units(ids,
                             get_priority(),
                             "ally_aid",
                             context,
                             snapshot.game_time,
                             k_aid_interval_seconds);
  if (claimed.empty()) {
    return;
  }
  AICommand move;
  move.type = AICommandType::MoveUnits;
  move.owner = get_priority();
  move.units = claimed;
  for (std::size_t i = 0; i < claimed.size(); ++i) {
    const float spread = static_cast<float>(i % 4) * 1.6F - 2.4F;
    const float rank = static_cast<float>(i / 4) * 1.6F;
    move.move_target_x.push_back(call->pos_x + spread);
    move.move_target_y.push_back(0.0F);
    move.move_target_z.push_back(call->pos_z + rank);
  }
  if (!qEnvironmentVariableIsEmpty("SOI_AI_TRACE")) {
    qInfo().nospace() << "SOI_AI_TRACE ally_aid player=" << context.player_id
                      << " ally=" << call->owner_id << " sent=" << move.units.size()
                      << " t=" << snapshot.game_time;
  }
  out_commands.push_back(std::move(move));
}

} // namespace Game::Systems::AI
