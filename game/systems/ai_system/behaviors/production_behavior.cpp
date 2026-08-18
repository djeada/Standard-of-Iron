#include "production_behavior.h"

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../../core/ownership_constants.h"
#include "../../nation_registry.h"
#include "../ai_base_manager.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/commander_catalog.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {
constexpr float k_rally_tolerance_sq = 4.0F;
}

void ProductionBehavior::execute(const AISnapshot& snapshot,
                                 AIContext& context,
                                 float delta_time,
                                 std::vector<AICommand>& out_commands) {
  m_production_timer += delta_time;

  const float effective_production_rate =
      context.strategy_config.production_rate_modifier *
      context.strategy_config.difficulty.production_rate_multiplier;
  float const production_interval = 1.5F / std::max(0.1F, effective_production_rate);

  if (m_production_timer < production_interval) {
    return;
  }
  m_production_timer = 0.0F;

  const auto* nation = context.nation;

  if (nation == nullptr) {
    return;
  }
  if (!nation->has_economy) {
    return;
  }

  constexpr int BUILDER_PRODUCTION_INTERVAL = 3;
  const int minimum_builders = std::max(1, context.macro_targets.builder_count - 1);
  const int desired_builders =
      std::max(minimum_builders, context.macro_targets.builder_count);

  bool should_produce_builder = false;

  if (context.builder_count < minimum_builders) {
    should_produce_builder = true;
  }

  else if (context.builder_count < desired_builders &&
           (m_production_counter % BUILDER_PRODUCTION_INTERVAL == 0)) {
    should_produce_builder = true;
  }

  const Game::Systems::TroopType* troop_type = nullptr;

  if (should_produce_builder) {

    troop_type = nation->get_troop(Game::Units::TroopType::Builder);
  }

  if (troop_type == nullptr) {
    bool produce_ranged = true;

    if (context.barracks_under_threat || context.state == AIState::Defending) {
      produce_ranged = (context.melee_count > context.ranged_count);
    } else {

      float const ranged_ratio =
          (context.total_units > 0)
              ? static_cast<float>(context.ranged_count) / context.total_units
              : 0.0F;

      const float target_ranged_ratio =
          std::clamp(0.5F + (context.strategy_config.defense_modifier -
                             context.strategy_config.aggression_modifier) *
                                0.1F,
                     0.25F,
                     0.75F);
      produce_ranged = (ranged_ratio < target_ranged_ratio);
      if (context.assembled_unit_count < context.macro_targets.assembly_size &&
          context.strategy_config.aggression_modifier > 1.0F) {
        produce_ranged = false;
      }
    }

    troop_type = produce_ranged ? nation->get_best_ranged_troop()
                                : nation->get_best_melee_troop();

    if (troop_type == nullptr) {
      troop_type = produce_ranged ? nation->get_best_melee_troop()
                                  : nation->get_best_ranged_troop();
    }
  }

  if (troop_type == nullptr) {
    return;
  }

  std::unordered_map<int, int> queued_by_base;
  queued_by_base.reserve(context.bases.size());
  for (const auto& base : context.bases) {
    queued_by_base[base.id] = base.queued_production;
  }

  std::unordered_map<Engine::Core::EntityID, const AIBase*> base_by_building;
  for (const auto& base : context.bases) {
    for (const auto building_id : base.buildings) {
      base_by_building[building_id] = &base;
    }
  }

  auto base_for = [&base_by_building](Engine::Core::EntityID building_id) {
    const auto it = base_by_building.find(building_id);
    return (it != base_by_building.end()) ? it->second : nullptr;
  };

  auto base_rank = [](const AIBase* base) {
    if (base == nullptr) {
      return 4;
    }
    if (base->under_threat) {
      return 0;
    }
    switch (base->role) {
    case BaseRole::Main:
      return 1;
    case BaseRole::Production:
      return 2;
    case BaseRole::Forward:
      return 3;
    case BaseRole::Defensive:
      return 4;
    }
    return 4;
  };

  std::vector<const EntitySnapshot*> barracks;
  barracks.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building || entity.spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    if (Game::Core::is_neutral_owner(entity.owner_id)) {
      continue;
    }
    if (!entity.production.has_component) {
      continue;
    }
    barracks.push_back(&entity);
  }

  std::stable_sort(barracks.begin(),
                   barracks.end(),
                   [&](const EntitySnapshot* a, const EntitySnapshot* b) {
                     const int rank_a = base_rank(base_for(a->id));
                     const int rank_b = base_rank(base_for(b->id));
                     if (rank_a != rank_b) {
                       return rank_a < rank_b;
                     }
                     return a->id < b->id;
                   });

  for (const auto* entity : barracks) {
    const auto& prod = entity->production;

    if (prod.produced_count >= prod.max_units) {
      continue;
    }

    const int max_queue_size = 5;
    int const total_in_queue = (prod.in_progress ? 1 : 0) + prod.queue_size;
    if (total_in_queue >= max_queue_size) {
      continue;
    }

    const auto* base = base_for(entity->id);
    if (base != nullptr) {
      auto& queued = queued_by_base[base->id];
      if (queued >= AIBaseManager::k_production_queue_per_base) {
        continue;
      }
      queued++;

      if (!prod.rally_set ||
          distance_squared(
              prod.rally_x, 0.0F, prod.rally_z, base->rally_x, 0.0F, base->rally_z) >
              k_rally_tolerance_sq) {
        AICommand rally_command;
        rally_command.type = AICommandType::SetRallyPoint;
        rally_command.building_id = entity->id;
        rally_command.rally_x = base->rally_x;
        rally_command.rally_z = base->rally_z;
        out_commands.push_back(std::move(rally_command));
      }
    }

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.building_id = entity->id;
    command.product_type = troop_type->unit_type;
    out_commands.push_back(std::move(command));

    m_production_counter++;
  }
}

auto ProductionBehavior::should_execute(const AISnapshot& snapshot,
                                        const AIContext& context) const -> bool {
  (void)snapshot;

  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }

  return context.total_units < context.max_troops_per_player;
}

} // namespace Game::Systems::AI
