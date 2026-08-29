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
#include "../ai_doctrine_catalog.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/commander_catalog.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {
constexpr float k_rally_tolerance_sq = 4.0F;

[[nodiscard]] auto is_recruitable_from(const Game::Systems::TroopType& troop,
                                       Game::Units::SpawnType building) -> bool {
  if (Game::Units::is_commander_troop(troop.unit_type)) {
    return false;
  }

  return Game::Systems::recruiting_building_for(troop.unit_type) == building;
}

[[nodiscard]] auto
standing_in_the_order_of_battle(const std::vector<std::string>& preferred,
                                const Game::Systems::TroopType& troop) -> int {
  const auto name = Game::Units::troop_typeToString(troop.unit_type);
  const auto it = std::find(preferred.begin(), preferred.end(), name);
  if (it == preferred.end()) {
    return static_cast<int>(preferred.size());
  }
  return static_cast<int>(std::distance(preferred.begin(), it));
}

[[nodiscard]] auto recruitable_candidates(const Game::Systems::Nation& nation,
                                          const Game::Systems::TroopType* preferred,
                                          const DoctrineRecruitment& recruitment,
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

  std::stable_sort(candidates.begin() + (candidates.empty() ? 0 : 1),
                   candidates.end(),
                   [&recruitment](const Game::Systems::TroopType* a,
                                  const Game::Systems::TroopType* b) {
                     const int rank_a =
                         standing_in_the_order_of_battle(recruitment.preferred, *a);
                     const int rank_b =
                         standing_in_the_order_of_battle(recruitment.preferred, *b);
                     if (rank_a != rank_b) {
                       return rank_a < rank_b;
                     }
                     return a->priority > b->priority;
                   });
  return candidates;
}

[[nodiscard]] auto cheapest_fighting_cost(const Game::Systems::Nation& nation) -> int {
  int cheapest = std::numeric_limits<int>::max();
  for (const auto& troop : nation.available_troops) {
    if (Game::Units::is_commander_troop(troop.unit_type) ||
        troop.unit_type == Game::Units::TroopType::Builder ||
        troop.unit_type == Game::Units::TroopType::Civilian ||
        !is_recruitable_from(troop, Game::Units::SpawnType::Barracks)) {
      continue;
    }
    cheapest = std::min(cheapest, troop.cost);
  }
  return cheapest == std::numeric_limits<int>::max() ? 0 : cheapest;
}

[[nodiscard]] auto
worth_waiting_for(const Game::Systems::TroopType& wanted,
                  const ProductionSnapshot& production,
                  const Game::Systems::ResourceAmounts& stock) -> bool {

  constexpr float k_close_enough = 0.6F;

  for (const auto type : Game::Systems::k_all_resource_types) {
    if (stock.get(type) < wanted.resource_costs.get(type)) {
      return false;
    }
  }
  if (wanted.cost <= 0) {
    return false;
  }
  return static_cast<float>(production.manpower_available) >=
         static_cast<float>(wanted.cost) * k_close_enough;
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

[[nodiscard]] auto arm_of(const Game::Systems::Nation& nation,
                          const Game::Systems::TroopType& troop) -> DoctrineArm {
  const auto spawn = Game::Units::spawn_typeFromTroopType(troop.unit_type);
  if (Game::Units::is_cavalry(spawn)) {
    return DoctrineArm::Cavalry;
  }
  if (spawn == Game::Units::SpawnType::Catapult ||
      spawn == Game::Units::SpawnType::Ballista) {
    return DoctrineArm::Siege;
  }
  return nation.is_ranged_unit(troop.unit_type) ? DoctrineArm::Missile
                                                : DoctrineArm::Infantry;
}

[[nodiscard]] auto share_of(const DoctrineRecruitment& recruitment,
                            DoctrineArm arm) -> float {
  const float cavalry = std::clamp(recruitment.cavalry_share, 0.0F, 1.0F);
  const float siege = std::clamp(recruitment.siege_share, 0.0F, 1.0F);
  const float foot = std::max(0.0F, 1.0F - cavalry - siege);
  const float missile = foot * std::clamp(recruitment.ranged_share, 0.0F, 1.0F);
  switch (arm) {
  case DoctrineArm::Cavalry:
    return cavalry;
  case DoctrineArm::Siege:
    return siege;
  case DoctrineArm::Missile:
    return missile;
  case DoctrineArm::Infantry:
    break;
  }
  return foot - missile;
}

[[nodiscard]] auto
default_recruitment(const AIContext& context) -> DoctrineRecruitment {
  DoctrineRecruitment recruitment;
  recruitment.ranged_share =
      std::clamp(0.5F + (context.strategy_config.defense_modifier -
                         context.strategy_config.aggression_modifier) *
                            0.1F,
                 0.25F,
                 0.75F);
  return recruitment;
}

[[nodiscard]] auto prefers(const DoctrineRecruitment& recruitment,
                           const Game::Systems::TroopType& troop) -> bool {
  const auto name = Game::Units::troop_typeToString(troop.unit_type);
  return std::find(recruitment.preferred.begin(), recruitment.preferred.end(), name) !=
         recruitment.preferred.end();
}

[[nodiscard]] auto units_under_arms(const AIContext& context, DoctrineArm arm) -> int {
  switch (arm) {
  case DoctrineArm::Infantry:
    return context.melee_count;
  case DoctrineArm::Missile:
    return context.ranged_count;
  case DoctrineArm::Cavalry:
    return context.cavalry_count;
  case DoctrineArm::Siege:
    return context.siege_count;
  }
  return 0;
}

[[nodiscard]] auto arm_is_at_establishment(const AIContext& context,
                                           const DoctrineRecruitment& recruitment,
                                           DoctrineArm arm) -> bool {
  const float target = share_of(recruitment, arm);
  if (target <= 0.0F) {

    return true;
  }
  const int total = context.melee_count + context.ranged_count + context.cavalry_count +
                    context.siege_count;

  constexpr int k_too_small_to_shape = 4;
  if (total < k_too_small_to_shape) {
    return false;
  }
  return static_cast<float>(units_under_arms(context, arm)) /
             static_cast<float>(total) >=
         target;
}

[[nodiscard]] auto in_no_position_to_be_choosy(const AIContext& context) -> bool {
  if (context.barracks_under_threat || context.state == AIState::Defending ||
      !context.buildings_under_attack.empty()) {
    return true;
  }

  constexpr int k_bare_garrison = 3;
  const int fighting = context.melee_count + context.ranged_count +
                       context.cavalry_count + context.siege_count;
  return fighting < k_bare_garrison;
}

[[nodiscard]] auto order_of_battle(const AIContext& context) -> DoctrineRecruitment {
  const auto* doctrine = context.strategy_config.doctrine;
  return doctrine != nullptr ? doctrine->recruitment : default_recruitment(context);
}

[[nodiscard]] auto
choose_recruit(const Game::Systems::Nation& nation,
               const AIContext& context) -> const Game::Systems::TroopType* {
  const DoctrineRecruitment recruitment = order_of_battle(context);

  const auto fielded = [&context](DoctrineArm arm) {
    return units_under_arms(context, arm);
  };
  const int total = context.melee_count + context.ranged_count + context.cavalry_count +
                    context.siege_count;

  DoctrineArm wanted = DoctrineArm::Infantry;
  float worst_gap = -1.0F;
  for (const auto arm : {DoctrineArm::Infantry,
                         DoctrineArm::Missile,
                         DoctrineArm::Cavalry,
                         DoctrineArm::Siege}) {
    const float target = share_of(recruitment, arm);
    if (target <= 0.0F) {
      continue;
    }
    const float have =
        total > 0 ? static_cast<float>(fielded(arm)) / static_cast<float>(total) : 0.0F;
    const float gap = target - have;
    if (gap > worst_gap) {
      worst_gap = gap;
      wanted = arm;
    }
  }

  if (context.barracks_under_threat || context.state == AIState::Defending) {

    wanted = context.melee_count > context.ranged_count ? DoctrineArm::Missile
                                                        : DoctrineArm::Infantry;
  }

  const Game::Systems::TroopType* best = nullptr;
  const Game::Systems::TroopType* fallback = nullptr;
  for (const auto& troop : nation.available_troops) {
    if (!is_recruitable_from(troop, Game::Units::SpawnType::Barracks) ||
        troop.unit_type == Game::Units::TroopType::Builder ||
        troop.unit_type == Game::Units::TroopType::Civilian) {
      continue;
    }
    if (fallback == nullptr || troop.priority > fallback->priority) {
      fallback = &troop;
    }
    if (arm_of(nation, troop) != wanted) {
      continue;
    }
    if (best == nullptr || prefers(recruitment, troop) ||
        (!prefers(recruitment, *best) && troop.priority > best->priority)) {
      best = &troop;
    }
  }
  return best != nullptr ? best : fallback;
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

  if (should_produce_builder && context.builder_count >= minimum_builders) {
    const auto* builder_troop = nation->get_troop(Game::Units::TroopType::Builder);
    const int soldier_cost = cheapest_fighting_cost(*nation);
    if (builder_troop != nullptr && soldier_cost > 0 &&
        context.recruitment_manpower_available - builder_troop->cost < soldier_cost) {
      should_produce_builder = false;
    }
  }

  const Game::Systems::TroopType* troop_type = nullptr;

  if (should_produce_builder) {

    troop_type = nation->get_troop(Game::Units::TroopType::Builder);
  }

  if (troop_type == nullptr) {
    troop_type = choose_recruit(*nation, context);
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

  const DoctrineRecruitment recruitment = order_of_battle(context);
  const auto candidates = recruitable_candidates(
      *nation, troop_type, recruitment, Game::Units::SpawnType::Barracks);

  for (const auto* entity : barracks) {
    const auto& prod = entity->production;

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

    const Game::Systems::TroopType* buying = nullptr;
    for (const auto* candidate : candidates) {
      if (arm_is_at_establishment(context, recruitment, arm_of(*nation, *candidate))) {
        continue;
      }
      if (affordable(*candidate, prod, snapshot.resources)) {
        buying = candidate;
        break;
      }
    }

    if (buying == nullptr && in_no_position_to_be_choosy(context)) {

      for (const auto* candidate : candidates) {
        if (affordable(*candidate, prod, snapshot.resources)) {
          buying = candidate;
          break;
        }
      }
    }

    if (buying == nullptr) {
      continue;
    }

    if (troop_type != nullptr && buying != troop_type &&
        worth_waiting_for(*troop_type, prod, snapshot.resources)) {
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
    if (room < Game::Systems::k_civilian_delivery_reserve_grant) {
      continue;
    }
    Recipient recipient;
    recipient.id = entity.id;
    recipient.x = entity.pos_x;
    recipient.z = entity.pos_z;
    recipient.room = room / Game::Systems::k_civilian_delivery_reserve_grant;
    recipients.push_back(recipient);
  }
  if (recipients.empty()) {
    return;
  }

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.spawn_type != Game::Units::SpawnType::Civilian) {
      continue;
    }

    if (entity.has_delivery_order) {
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

  return context.population_headroom() > 0;
}

} // namespace Game::Systems::AI
