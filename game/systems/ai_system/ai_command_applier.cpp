#include "ai_command_applier.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../game_config.h"
#include "../../units/troop_config.h"
#include "../command_service.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/troop_type.h"

#include <QVector3D>
#include <cstddef>
#include <qvectornd.h>
#include <vector>

namespace Game::Systems::AI {

void AICommandApplier::apply(Engine::Core::World &world, int aiOwnerId,
                             const std::vector<AICommand> &commands) {

  for (const auto &command : commands) {
    switch (command.type) {

    case AICommandType::MoveUnits: {
      if (command.units.empty()) {
        break;
      }

      std::vector<float> expanded_x;
      std::vector<float> expanded_y;
      std::vector<float> expanded_z;

      if (command.moveTargetX.size() != command.units.size()) {
        replicateLastTargetIfNeeded(command.moveTargetX, command.moveTargetY,
                                    command.moveTargetZ, command.units.size(),
                                    expanded_x, expanded_y, expanded_z);
      } else {
        expanded_x = command.moveTargetX;
        expanded_y = command.moveTargetY;
        expanded_z = command.moveTargetZ;
      }

      if (expanded_x.empty()) {
        break;
      }

      std::vector<Engine::Core::EntityID> owned_units;
      std::vector<QVector3D> owned_targets;
      owned_units.reserve(command.units.size());
      owned_targets.reserve(command.units.size());

      for (std::size_t idx = 0; idx < command.units.size(); ++idx) {
        auto entity_id = command.units[idx];
        auto *entity = world.getEntity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if ((unit == nullptr) || unit->owner_id != aiOwnerId) {
          continue;
        }

        owned_units.push_back(entity_id);
        owned_targets.emplace_back(expanded_x[idx], expanded_y[idx],
                                   expanded_z[idx]);
      }

      if (owned_units.empty()) {
        break;
      }

      CommandService::MoveOptions opts;
      opts.allowDirectFallback = true;
      opts.clearAttackIntent = false;
      opts.groupMove = owned_units.size() > 1;
      CommandService::moveUnits(world, owned_units, owned_targets, opts);
      break;
    }

    case AICommandType::AttackTarget: {
      if (command.units.empty() || command.target_id == 0) {
        break;
      }

      std::vector<Engine::Core::EntityID> owned_units;
      owned_units.reserve(command.units.size());

      for (auto entity_id : command.units) {
        auto *entity = world.getEntity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if ((unit == nullptr) || unit->owner_id != aiOwnerId) {
          continue;
        }

        owned_units.push_back(entity_id);
      }

      if (owned_units.empty()) {
        break;
      }

      CommandService::attack_target(world, owned_units, command.target_id,
                                    command.should_chase);
      break;
    }

    case AICommandType::StartProduction: {
      auto *entity = world.getEntity(command.buildingId);
      if (entity == nullptr) {
        break;
      }

      auto *production =
          entity->getComponent<Engine::Core::ProductionComponent>();
      if (production == nullptr) {
        break;
      }

      if (production->in_progress) {
        break;
      }

      auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
      if ((unit != nullptr) && unit->owner_id != aiOwnerId) {
        break;
      }

      int const current_troops =
          Engine::Core::World::countTroopsForPlayer(aiOwnerId);
      int const max_troops =
          Game::GameConfig::instance().getMaxTroopsPerPlayer();
      Game::Units::TroopType const product_type = production->product_type;
      int const individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              product_type);
      if (current_troops + individuals_per_unit > max_troops) {
        break;
      }

      production->product_type = command.product_type;

      production->time_remaining = production->build_time;
      production->in_progress = true;

      break;
    }
    }
  }
}

} // namespace Game::Systems::AI
