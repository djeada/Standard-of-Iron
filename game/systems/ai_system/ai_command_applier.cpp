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

      if (command.move_target_x.size() != command.units.size()) {
        replicate_last_target_if_needed(
            command.move_target_x, command.move_target_y, command.move_target_z,
            command.units.size(), expanded_x, expanded_y, expanded_z);
      } else {
        expanded_x = command.move_target_x;
        expanded_y = command.move_target_y;
        expanded_z = command.move_target_z;
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
        auto *entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto *unit = entity->get_component<Engine::Core::UnitComponent>();
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
      opts.allow_direct_fallback = true;
      opts.clear_attack_intent = false;
      opts.group_move = owned_units.size() > 1;
      CommandService::move_units(world, owned_units, owned_targets, opts);
      break;
    }

    case AICommandType::AttackTarget: {
      if (command.units.empty() || command.target_id == 0) {
        break;
      }

      std::vector<Engine::Core::EntityID> owned_units;
      owned_units.reserve(command.units.size());

      for (auto entity_id : command.units) {
        auto *entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto *unit = entity->get_component<Engine::Core::UnitComponent>();
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
      auto *entity = world.get_entity(command.building_id);
      if (entity == nullptr) {
        break;
      }

      auto *production =
          entity->get_component<Engine::Core::ProductionComponent>();
      if (production == nullptr) {
        break;
      }

      if (production->in_progress) {
        break;
      }

      auto *unit = entity->get_component<Engine::Core::UnitComponent>();
      if ((unit != nullptr) && unit->owner_id != aiOwnerId) {
        break;
      }

      int const current_troops =
          Engine::Core::World::count_troops_for_player(aiOwnerId);
      int const max_troops =
          Game::GameConfig::instance().get_max_troops_per_player();
      Game::Units::TroopType const product_type = production->product_type;
      int const production_cost =
          Game::Units::TroopConfig::instance().getProductionCost(product_type);
      if (current_troops + production_cost > max_troops) {
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
