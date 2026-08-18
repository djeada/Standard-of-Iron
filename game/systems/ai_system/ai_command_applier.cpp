#include "ai_command_applier.h"

#include <QVector3D>
#include <qvectornd.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "../../command/command.h"
#include "../../command/command_dispatcher.h"
#include "../../command/command_queue.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../game_config.h"
#include "../../map/terrain_service.h"
#include "../../session/session_context.h"
#include "../../units/troop_config.h"
#include "../combat_system/combat_utils.h"
#include "../command_service.h"
#include "../construction_cost_catalog.h"
#include "../owner_queries.h"
#include "../player_resource_registry.h"
#include "../production_service.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {

void submit(Engine::Core::World& world, int owner_id, Game::Command::Payload payload) {
  Game::Command::submit(world, Game::Command::Source::AI, owner_id, std::move(payload));
}
} // namespace

void AICommandApplier::apply(Engine::Core::World& world,
                             int ai_owner_id,
                             const std::vector<AICommand>& commands) {

  for (const auto& command : commands) {
    switch (command.type) {

    case AICommandType::MoveUnits: {
      if (command.units.empty()) {
        break;
      }

      std::vector<float> expanded_x;
      std::vector<float> expanded_y;
      std::vector<float> expanded_z;

      if (command.move_target_x.size() != command.units.size()) {
        replicate_last_target_if_needed(command.move_target_x,
                                        command.move_target_y,
                                        command.move_target_z,
                                        command.units.size(),
                                        expanded_x,
                                        expanded_y,
                                        expanded_z);
      } else {
        expanded_x = command.move_target_x;
        expanded_y = command.move_target_y;
        expanded_z = command.move_target_z;
      }

      if (expanded_x.empty()) {
        break;
      }

      Game::Command::Move move;
      move.kind = MoveOrderKind::PlannerMove;
      move.units = command.units;
      move.targets.reserve(command.units.size());
      for (std::size_t idx = 0; idx < command.units.size(); ++idx) {
        move.targets.emplace_back(expanded_x[idx], expanded_y[idx], expanded_z[idx]);
      }
      submit(world, ai_owner_id, std::move(move));
      break;
    }

    case AICommandType::AttackTarget: {
      if (command.units.empty() || command.target_id == 0) {
        break;
      }

      auto* target = world.get_entity(command.target_id);
      std::vector<Engine::Core::EntityID> attackers;
      attackers.reserve(command.units.size());
      for (const auto unit_id : command.units) {
        if (!Game::Systems::Combat::melee_walled_off_from(world.get_entity(unit_id),
                                                          target)) {
          attackers.push_back(unit_id);
        }
      }
      if (attackers.empty()) {
        break;
      }

      submit(world,
             ai_owner_id,
             Game::Command::AttackTarget{.units = std::move(attackers),
                                         .target = command.target_id,
                                         .should_chase = command.should_chase});
      break;
    }

    case AICommandType::StartProduction: {

      auto* entity = world.get_entity(command.building_id);
      auto* production =
          entity != nullptr ? entity->get_component<Engine::Core::ProductionComponent>()
                            : nullptr;
      const auto* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (production == nullptr || unit == nullptr || unit->owner_id != ai_owner_id) {
        break;
      }
      if (Game::Systems::ProductionService::can_start_production(
              world, command.building_id, command.product_type) !=
          Game::Systems::ProductionResult::Success) {
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::Produce{.building = command.building_id,
                                    .product = command.product_type});
      break;
    }

    case AICommandType::SetRallyPoint: {
      submit(world,
             ai_owner_id,
             Game::Command::SetRallyPoint{
                 .building = command.building_id,
                 .position = QVector3D(command.rally_x, 0.0F, command.rally_z)});
      break;
    }

    case AICommandType::TriggerCommanderRally:
    case AICommandType::TriggerCommanderAura: {
      const auto ability = command.type == AICommandType::TriggerCommanderRally
                               ? Game::Command::CommanderAbility::Rally
                               : Game::Command::CommanderAbility::Aura;
      for (auto entity_id : command.units) {
        submit(world,
               ai_owner_id,
               Game::Command::UseCommanderAbility{.commander = entity_id,
                                                  .ability = ability});
      }
      break;
    }

    case AICommandType::StartBuilderConstruction: {
      if (command.units.empty() || command.construction_type == nullptr) {
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::StartConstruction{
                 .units = command.units,
                 .construction_type = command.construction_type,
                 .site = QVector3D(
                     command.construction_site_x, 0.0F, command.construction_site_z)});
      break;
    }

    case AICommandType::StartBuilderHarvest: {
      if (command.units.empty() || command.construction_type == nullptr ||
          command.resource_target_id == 0) {
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::StartHarvest{
                 .units = command.units,
                 .construction_type = command.construction_type,
                 .resource_target = command.resource_target_id,
                 .site = QVector3D(
                     command.construction_site_x, 0.0F, command.construction_site_z)});
      break;
    }
    }
  }
}

} // namespace Game::Systems::AI
