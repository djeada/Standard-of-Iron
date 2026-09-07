#include "squad_discipline_behavior.h"

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../squad_service.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_interval = 5.0F;

auto is_understrength(const EntitySnapshot& entity) -> bool {
  return entity.squad_establishment > 1 &&
         entity.squad_strength < entity.squad_establishment;
}

auto is_mergeable(const EntitySnapshot& entity, const AIContext& context) -> bool {
  if (entity.is_building || entity.is_commander || entity.health <= 0) {
    return false;
  }
  if (!is_combat_role_unit(entity)) {
    return false;
  }
  if (entity.is_assault || is_harass_unit(entity.id, context)) {
    return false;
  }

  return !std::any_of(context.wave.members.begin(),
                      context.wave.members.end(),
                      [&entity](Engine::Core::EntityID id) { return id == entity.id; });
}

} // namespace

void SquadDisciplineBehavior::execute(const AISnapshot& snapshot,
                                      AIContext& context,
                                      float delta_time,
                                      std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_interval) {
    return;
  }
  m_timer = 0.0F;

  std::unordered_map<Game::Units::SpawnType, std::vector<const EntitySnapshot*>>
      by_type;
  for (const auto& entity : snapshot.friendly_units) {
    if (!is_mergeable(entity, context) || !is_understrength(entity)) {
      continue;
    }
    by_type[entity.spawn_type].push_back(&entity);
  }

  const float radius_sq = SquadService::k_merge_radius * SquadService::k_merge_radius;

  for (auto& [spawn_type, squads] : by_type) {
    if (squads.size() < 2U) {
      continue;
    }
    std::sort(squads.begin(),
              squads.end(),
              [](const EntitySnapshot* a, const EntitySnapshot* b) {
                return a->squad_strength < b->squad_strength;
              });

    std::vector<bool> taken(squads.size(), false);
    for (std::size_t i = 0; i < squads.size(); ++i) {
      if (taken[i]) {
        continue;
      }
      for (std::size_t j = i + 1; j < squads.size(); ++j) {
        if (taken[j]) {
          continue;
        }
        if (distance_squared(squads[i]->pos_x,
                             0.0F,
                             squads[i]->pos_z,
                             squads[j]->pos_x,
                             0.0F,
                             squads[j]->pos_z) > radius_sq) {
          continue;
        }

        AICommand command;
        command.type = AICommandType::MergeSquads;
        command.units.push_back(squads[i]->id);
        command.units.push_back(squads[j]->id);
        out_commands.push_back(std::move(command));
        taken[i] = true;
        taken[j] = true;
        break;
      }
    }
  }
}

auto SquadDisciplineBehavior::should_execute(const AISnapshot& snapshot,
                                             const AIContext& context) const -> bool {
  int understrength = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (is_mergeable(entity, context) && is_understrength(entity)) {
      ++understrength;
      if (understrength >= 2) {
        return true;
      }
    }
  }
  return false;
}

} // namespace Game::Systems::AI
