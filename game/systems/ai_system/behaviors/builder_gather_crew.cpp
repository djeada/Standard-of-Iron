#include "builder_gather_crew.h"

#include <algorithm>
#include <utility>

#include "../ai_utils.h"
#include "builder_affordability.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

void GatherCrew::manage(const AISnapshot& snapshot,
                        const AIContext& context,
                        bool reclaim_one,
                        BuilderPool& pool,
                        std::vector<AICommand>& out_commands) {
  std::vector<Engine::Core::EntityID> gatherers;
  int builder_total = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }
    ++builder_total;
    if (entity.builder_production.auto_gather) {
      gatherers.push_back(entity.id);
    }
  }
  if (builder_total == 0) {
    return;
  }

  const int construction_crew = std::clamp(2 + (builder_total / 6), 2, 5);
  const bool forage_in_reach =
      std::any_of(snapshot.resource_nodes.begin(),
                  snapshot.resource_nodes.end(),
                  [&context](const ResourceNodeSnapshot& node) {
                    return forage_is_within_reach(context, node.pos_x, node.pos_z);
                  });
  const int desired_gatherers =
      forage_in_reach
          ? std::max(0, builder_total - construction_crew - (reclaim_one ? 1 : 0))
          : 0;
  const char* priority = neediest_stockpile(
      snapshot, static_cast<int>(context.buildings.size()), planned_wall_wood(context));

  if (static_cast<int>(gatherers.size()) > desired_gatherers) {
    AICommand release;
    release.type = AICommandType::SetAutoGather;
    release.auto_gather_active = false;
    for (int i = desired_gatherers; i < static_cast<int>(gatherers.size()); ++i) {
      release.units.push_back(gatherers[static_cast<std::size_t>(i)]);
    }
    out_commands.push_back(std::move(release));
    return;
  }

  constexpr float k_priority_hold_seconds = 45.0F;
  const bool may_switch =
      snapshot.game_time - m_priority_time >= k_priority_hold_seconds;
  const bool priority_changed = priority != m_priority && may_switch;
  if (priority_changed || m_priority == nullptr) {
    m_priority = priority;
    m_priority_time = snapshot.game_time;
  }

  AICommand order;
  order.type = AICommandType::SetAutoGather;
  order.auto_gather_active = true;
  order.construction_type = m_priority;
  if (priority_changed) {

    order.units = gatherers;
  }
  while (static_cast<int>(gatherers.size()) + static_cast<int>(order.units.size()) <
             desired_gatherers &&
         !pool.empty()) {
    order.units.push_back(pool.take_last());
  }
  if (!order.units.empty()) {
    out_commands.push_back(std::move(order));
  }
}

} // namespace Game::Systems::AI
