#include "command_dispatcher.h"

#include <type_traits>
#include <variant>
#include <vector>

#include "../core/component_core.h"
#include "../core/world.h"
#include "../formation/army_formation_service.h"
#include "../systems/economy/production_service.h"
#include "../systems/movement/command_service.h"
#include "../systems/siege_tower_system.h"
#include "../units/spawn_type.h"
#include "command_handlers.h"

namespace Game::Command {

using Engine::Core::World;

void dispatch(World& world, const Command& command) {
  using namespace handlers;
  std::visit(
      [&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<T, Move>) {
          apply_move(world, payload);
        } else if constexpr (std::is_same_v<T, AttackTarget>) {
          Game::Formation::ArmyFormationService::release(world, payload.units);
          // A siege tower sent at an enemy wall rolls up to dock against it;
          // everything else attacks.
          std::vector<Engine::Core::EntityID> attackers;
          attackers.reserve(payload.units.size());
          for (auto const id : payload.units) {
            auto const* unit = world.try_get<Engine::Core::UnitComponent>(id);
            if (unit != nullptr &&
                unit->spawn_type == Game::Units::SpawnType::SiegeTower) {
              if (auto const spot = Game::Systems::siege_tower_dock_approach(
                      world, id, payload.target)) {
                Game::Systems::CommandService::move_unit(world, id, *spot);
              }
              continue;
            }
            attackers.push_back(id);
          }
          if (!attackers.empty()) {
            Game::Systems::CommandService::attack_target(
                world, attackers, payload.target, payload.should_chase);
          }
        } else if constexpr (std::is_same_v<T, Stop>) {
          apply_stop(world, payload);
        } else if constexpr (std::is_same_v<T, SetHold>) {
          apply_hold(world, payload);
        } else if constexpr (std::is_same_v<T, SetGuard>) {
          apply_guard(world, payload);
        } else if constexpr (std::is_same_v<T, SetRunMode>) {
          apply_run_mode(world, payload);
        } else if constexpr (std::is_same_v<T, Patrol>) {
          apply_patrol(world, payload);
        } else if constexpr (std::is_same_v<T, SetRallyPoint>) {
          Game::Systems::ProductionService::set_rally_point(
              world, payload.building, payload.position.x(), payload.position.z());
        } else if constexpr (std::is_same_v<T, SetGateMode>) {
          apply_gate_mode(world, payload);
        } else if constexpr (std::is_same_v<T, SetAutoGather>) {
          apply_auto_gather(world, payload);
        } else if constexpr (std::is_same_v<T, Produce>) {
          Game::Systems::ProductionService::start_production(
              world, payload.building, payload.product);
        } else if constexpr (std::is_same_v<T, Trade>) {
          apply_trade(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, AllyTribute>) {
          apply_ally_tribute(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, AllyCall>) {
          apply_ally_call(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, AllyAppealAnswer>) {
          apply_ally_appeal_answer(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, RollStones>) {
          apply_roll_stones(world, payload);
        } else if constexpr (std::is_same_v<T, UseCommanderAbility>) {
          apply_commander_ability(world, payload);
        } else if constexpr (std::is_same_v<T, SetFormationMode>) {
          apply_formation_mode(world, payload);
        } else if constexpr (std::is_same_v<T, DeployFormation>) {
          apply_deploy_formation(world, payload);
        } else if constexpr (std::is_same_v<T, ReleaseFormation>) {
          apply_release_formation(world, payload);
        } else if constexpr (std::is_same_v<T, StartConstruction>) {
          apply_start_construction(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, StartHarvest>) {
          apply_start_harvest(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, DeliverCivilians>) {
          apply_deliver_civilians(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, DivideSquads>) {
          apply_divide_squads(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, MergeSquads>) {
          apply_merge_squads(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, RepairStructure>) {
          apply_repair_structure(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, DismantleStructure>) {
          apply_dismantle_structure(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, PlaceWallPlan>) {
          apply_place_wall_plan(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, PlaceBuilding>) {
          apply_place_building(world, command.owner_id, payload);
        }
      },
      command.payload);
}

} // namespace Game::Command
