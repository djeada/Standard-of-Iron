#include "production_behavior.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../../core/ownership_constants.h"
#include "../../civilian_delivery_system.h"
#include "../../nation_registry.h"
#include "../../production_service.h"
#include "../ai_base_manager.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/commander_catalog.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {
constexpr float k_rally_tolerance_sq = 4.0F;

// Commanders sit at the top of every nation's priority list, and
// `ProductionService` refuses to recruit one from a barracks. Asking for the
// highest-priority troop therefore produced a command that was silently dropped
// on every single decision, which is why Rome never fielded an army at all.
[[nodiscard]] auto is_recruitable_from(const Game::Systems::TroopType& troop,
                                       Game::Units::SpawnType building) -> bool {
  if (Game::Units::is_commander_troop(troop.unit_type)) {
    return false;
  }

  // Civilians come from a home and healers from a temple. Asking a barracks for
  // one is refused, and the computer used to fall all the way through its
  // priority list to the cheapest troop it could name - a one-manpower civilian
  // - and then ask the barracks for that, every decision, forever.
  return Game::Systems::recruiting_building_for(troop.unit_type) == building;
}

// The preferred troop first, then everything else the nation can recruit, best
// priority first. A plan with one candidate stalls the moment that candidate
// is unaffordable: the barracks opens with a fixed manpower budget, and once
// the computer had spent it on builders it re-asked for another builder every
// decision for the rest of the match instead of recruiting something cheaper.
[[nodiscard]] auto recruitable_candidates(const Game::Systems::Nation& nation,
                                          const Game::Systems::TroopType* preferred,
                                          Game::Units::SpawnType building)
    -> std::vector<const Game::Systems::TroopType*> {
  std::vector<const Game::Systems::TroopType*> candidates;
  candidates.reserve(nation.available_troops.size());
  if (preferred != nullptr && is_recruitable_from(*preferred, building)) {
    candidates.push_back(preferred);
  }
  for (const auto& troop : nation.available_troops) {
    if (!is_recruitable_from(troop, building) || &troop == preferred) {
      continue;
    }
    candidates.push_back(&troop);
  }
  std::stable_sort(
      candidates.begin() + (candidates.empty() ? 0 : 1),
      candidates.end(),
      [](const Game::Systems::TroopType* a, const Game::Systems::TroopType* b) {
        return a->priority > b->priority;
      });
  return candidates;
}

[[nodiscard]] auto affordable(const Game::Systems::TroopType& troop,
                              const ProductionSnapshot& production,
                              const Game::Systems::ResourceAmounts& stock) -> bool {
  if (production.manpower_available < troop.cost) {
    return false;
  }
  for (const auto type : Game::Systems::k_all_resource_types) {
    if (stock.get(type) < troop.resource_costs.get(type)) {
      return false;
    }
  }
  return true;
}
} // namespace

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

  const auto candidates =
      recruitable_candidates(*nation, troop_type, Game::Units::SpawnType::Barracks);

  for (const auto* entity : barracks) {
    const auto& prod = entity->production;

    // `produced_count` is a home's civilian tally, and only a home is capped by
    // it - `ProductionService` and `ProductionSystem` both enforce that limit
    // for homes alone. Applying it to a barracks retired the building for good
    // once its opening manpower had been spent, which is where the computer's
    // army stopped for the rest of the match.

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

    // A rally point is a standing order and costs nothing; deciding what to
    // recruit is a purchase. Only the purchase depends on what the building can
    // afford.
    const Game::Systems::TroopType* buying = nullptr;
    for (const auto* candidate : candidates) {
      if (affordable(*candidate, prod, snapshot.resources)) {
        buying = candidate;
        break;
      }
    }
    if (buying == nullptr) {
      continue;
    }

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.building_id = entity->id;
    command.product_type = buying->unit_type;
    out_commands.push_back(std::move(command));

    m_production_counter++;
  }

  queue_civilians_at_homes(snapshot, out_commands);
  deliver_idle_civilians(snapshot, out_commands);
}

// A civilian is manpower on legs: it refills a barracks only when it walks into
// one. The computer produced them and left them standing in the settlement, so
// the barracks budget it spent on its opening builders was all it would ever
// have. Send every civilian with nothing else to do to the nearest barracks
// that still has room for it.
void ProductionBehavior::deliver_idle_civilians(
    const AISnapshot& snapshot, std::vector<AICommand>& out_commands) const {
  struct Recipient {
    Engine::Core::EntityID id = 0;
    float x = 0.0F;
    float z = 0.0F;
    int room = 0;
  };

  std::vector<Recipient> recipients;
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building || entity.spawn_type != Game::Units::SpawnType::Barracks ||
        !entity.production.has_component) {
      continue;
    }
    const int room = entity.production.max_units - entity.production.manpower_available;
    if (room < Game::Systems::k_civilian_delivery_population_grant) {
      continue;
    }
    Recipient recipient;
    recipient.id = entity.id;
    recipient.x = entity.pos_x;
    recipient.z = entity.pos_z;
    recipient.room = room / Game::Systems::k_civilian_delivery_population_grant;
    recipients.push_back(recipient);
  }
  if (recipients.empty()) {
    return;
  }

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.spawn_type != Game::Units::SpawnType::Civilian) {
      continue;
    }
    // Anything already walking is either delivering itself or busy.
    if (entity.movement.has_component && entity.movement.has_target) {
      continue;
    }

    Recipient* nearest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (auto& recipient : recipients) {
      if (recipient.room <= 0) {
        continue;
      }
      const float distance = distance_squared(
          entity.pos_x, 0.0F, entity.pos_z, recipient.x, 0.0F, recipient.z);
      if (distance < best) {
        best = distance;
        nearest = &recipient;
      }
    }
    if (nearest == nullptr) {
      return;
    }

    AICommand command;
    command.type = AICommandType::DeliverCivilians;
    command.building_id = nearest->id;
    command.units.push_back(entity.id);
    out_commands.push_back(std::move(command));
    --nearest->room;
  }
}

// A barracks opens with a fixed manpower budget and refills only when a
// civilian walks in from a home. The computer built homes and then never
// staffed one, so the budget it spent on its opening builders was the entire
// manpower it would ever have: no civilians, no refill, no army, for the whole
// match. Keeping every home at work is the economy this game is built around.
void ProductionBehavior::queue_civilians_at_homes(
    const AISnapshot& snapshot, std::vector<AICommand>& out_commands) const {
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building || entity.spawn_type != Game::Units::SpawnType::Home) {
      continue;
    }
    if (!entity.production.has_component ||
        Game::Core::is_neutral_owner(entity.owner_id)) {
      continue;
    }

    const auto& prod = entity.production;
    constexpr int k_home_queue_size = 3;
    int const committed =
        prod.produced_count + (prod.in_progress ? 1 : 0) + prod.queue_size;
    if (committed >= prod.max_units) {
      continue;
    }
    if ((prod.in_progress ? 1 : 0) + prod.queue_size >= k_home_queue_size) {
      continue;
    }

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.building_id = entity.id;
    command.product_type = Game::Units::TroopType::Civilian;
    out_commands.push_back(std::move(command));
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
