#pragma once

#include <vector>

#include "../core/world.h"
#include "command.h"

namespace Game::Command::handlers {

template <typename Fn>
void for_each_subject(Engine::Core::World& world,
                      const std::vector<Engine::Core::EntityID>& units,
                      Fn&& fn) {
  for (const Engine::Core::EntityID id : units) {
    if (auto* entity = world.get_entity(id)) {
      fn(*entity);
    }
  }
}

void apply_move(Engine::Core::World& world, const Move& move);
void apply_stop(Engine::Core::World& world, const Stop& stop);
void apply_gate_mode(Engine::Core::World& world, const SetGateMode& order);
void apply_hold(Engine::Core::World& world, const SetHold& hold);
void apply_guard(Engine::Core::World& world, const SetGuard& guard);
void apply_run_mode(Engine::Core::World& world, const SetRunMode& run);
void apply_patrol(Engine::Core::World& world, const Patrol& patrol);
void apply_commander_ability(Engine::Core::World& world,
                             const UseCommanderAbility& order);
void apply_formation_mode(Engine::Core::World& world, const SetFormationMode& order);
void apply_deploy_formation(Engine::Core::World& world, const DeployFormation& order);
void apply_release_formation(Engine::Core::World& world, const ReleaseFormation& order);
void apply_divide_squads(Engine::Core::World& world,
                         int owner_id,
                         const DivideSquads& order);
void apply_merge_squads(Engine::Core::World& world,
                        int owner_id,
                        const MergeSquads& order);

void apply_auto_gather(Engine::Core::World& world, const SetAutoGather& order);
void apply_start_construction(Engine::Core::World& world,
                              int owner_id,
                              const StartConstruction& order);
void apply_start_harvest(Engine::Core::World& world,
                         int owner_id,
                         const StartHarvest& order);
void apply_deliver_civilians(Engine::Core::World& world,
                             int owner_id,
                             const DeliverCivilians& order);
void apply_repair_structure(Engine::Core::World& world,
                            int owner_id,
                            const RepairStructure& order);
void apply_dismantle_structure(Engine::Core::World& world,
                               int owner_id,
                               const DismantleStructure& order);
void apply_place_wall_plan(Engine::Core::World& world,
                           int owner_id,
                           const PlaceWallPlan& order);
void apply_place_building(Engine::Core::World& world,
                          int owner_id,
                          const PlaceBuilding& order);

void apply_trade(Engine::Core::World& world, int owner_id, const Trade& trade);
void apply_ally_tribute(Engine::Core::World& world,
                        int owner_id,
                        const AllyTribute& tribute);
void apply_ally_call(Engine::Core::World& world, int owner_id, const AllyCall& call);
void apply_ally_appeal_answer(Engine::Core::World& world,
                              int owner_id,
                              const AllyAppealAnswer& answer);
void apply_roll_stones(Engine::Core::World& world, const RollStones& order);

} // namespace Game::Command::handlers
