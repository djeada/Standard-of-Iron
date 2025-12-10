#include "production_behavior.h"
#include "../../../core/ownership_constants.h"
#include "../../nation_registry.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include <utility>
#include <vector>

namespace Game::Systems::AI {

void ProductionBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                                 float deltaTime,
                                 std::vector<AICommand> &outCommands) {
  m_productionTimer += deltaTime;
  if (m_productionTimer < 1.5F) {
    return;
  }
  m_productionTimer = 0.0F;

  static int const exec_counter = 0;

  auto &nation_registry = Game::Systems::NationRegistry::instance();
  const auto *nation = nation_registry.getNationForPlayer(context.player_id);

  if (nation == nullptr) {

    static int const log_counter = 0;
    return;
  }

  bool produce_ranged = true;

  if (context.barracksUnderThreat || context.state == AIState::Defending) {
    produce_ranged = (context.meleeCount > context.rangedCount);
  } else {

    float const ranged_ratio =
        (context.total_units > 0)
            ? static_cast<float>(context.rangedCount) / context.total_units
            : 0.0F;
    produce_ranged = (ranged_ratio < 0.6F);
  }

  const Game::Systems::TroopType *troop_type =
      produce_ranged ? nation->getBestRangedTroop()
                     : nation->getBestMeleeTroop();

  if (troop_type == nullptr) {
    troop_type = produce_ranged ? nation->getBestMeleeTroop()
                                : nation->getBestRangedTroop();
  }

  if (troop_type == nullptr) {
    static int const log_counter = 0;
    return;
  }

  for (const auto &entity : snapshot.friendlies) {
    if (!entity.isBuilding ||
        entity.spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    if (Game::Core::isNeutralOwner(entity.owner_id)) {
      continue;
    }

    static int const log_counter = 0;

    if (!entity.production.hasComponent) {
      continue;
    }

    const auto &prod = entity.production;

    if (prod.produced_count >= prod.max_units) {
      continue;
    }

    const int max_queue_size = 5;
    int const total_in_queue = (prod.in_progress ? 1 : 0) + prod.queueSize;
    if (total_in_queue >= max_queue_size) {
      continue;
    }

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.buildingId = entity.id;
    command.product_type = troop_type->unit_type;
    outCommands.push_back(std::move(command));

    m_productionCounter++;
  }
}

auto ProductionBehavior::should_execute(
    const AISnapshot &snapshot, const AIContext &context) const -> bool {
  (void)snapshot;

  return context.total_units < context.max_troops_per_player;
}

} // namespace Game::Systems::AI
