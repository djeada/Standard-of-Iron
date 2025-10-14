#include "production_behavior.h"
#include "../../nation_registry.h"
#include "../ai_tactical.h"

namespace Game::Systems::AI {

void ProductionBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                                 float deltaTime,
                                 std::vector<AICommand> &outCommands) {
  m_productionTimer += deltaTime;
  if (m_productionTimer < 1.5f)
    return;
  m_productionTimer = 0.0f;

  static int execCounter = 0;

  auto &nationRegistry = Game::Systems::NationRegistry::instance();
  const auto *nation = nationRegistry.getNationForPlayer(context.playerId);

  if (!nation) {

    static int logCounter = 0;
    return;
  }

  bool produceRanged = true;

  if (context.barracksUnderThreat || context.state == AIState::Defending) {
    produceRanged = (context.meleeCount > context.rangedCount);
  } else {

    float rangedRatio =
        (context.totalUnits > 0)
            ? static_cast<float>(context.rangedCount) / context.totalUnits
            : 0.0f;
    produceRanged = (rangedRatio < 0.6f);
  }

  const Game::Systems::TroopType *troopType = produceRanged
                                                  ? nation->getBestRangedTroop()
                                                  : nation->getBestMeleeTroop();

  if (!troopType) {
    troopType = produceRanged ? nation->getBestMeleeTroop()
                              : nation->getBestRangedTroop();
  }

  if (!troopType) {
    static int logCounter = 0;
    return;
  }

  for (const auto &entity : snapshot.friendlies) {
    if (!entity.isBuilding || entity.unitType != "barracks")
      continue;

    static int logCounter = 0;

    if (!entity.production.hasComponent)
      continue;

    const auto &prod = entity.production;

    if (prod.inProgress)
      continue;

    if (prod.producedCount >= prod.maxUnits)
      continue;

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.buildingId = entity.id;
    command.productType = troopType->unitType;
    outCommands.push_back(std::move(command));

    m_productionCounter++;
  }
}

bool ProductionBehavior::shouldExecute(const AISnapshot &snapshot,
                                       const AIContext &context) const {
  (void)snapshot;

  if (context.totalUnits >= context.maxTroopsPerPlayer)
    return false;

  return true;
}

} // namespace Game::Systems::AI
