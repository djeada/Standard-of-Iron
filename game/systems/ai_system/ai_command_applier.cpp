#include "ai_command_applier.h"

#include <QVector3D>
#include <qvectornd.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../game_config.h"
#include "../../units/troop_config.h"
#include "../command_service.h"
#include "../construction_cost_catalog.h"
#include "../player_resource_registry.h"
#include "../production_service.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {

constexpr const char* BUILDING_TYPE_HOME = "home";
constexpr const char* BUILDING_TYPE_DEFENSE_TOWER = "defense_tower";
constexpr const char* BUILDING_TYPE_BARRACKS = "barracks";
constexpr const char* BUILDING_TYPE_MARKETPLACE = "marketplace";

constexpr float BUILD_TIME_HOME = 20.0F;
constexpr float BUILD_TIME_DEFENSE_TOWER = 25.0F;
constexpr float BUILD_TIME_BARRACKS = 30.0F;
constexpr float BUILD_TIME_MARKETPLACE = 25.0F;
constexpr float BUILD_TIME_DEFAULT = 20.0F;
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

      std::vector<Engine::Core::EntityID> owned_units;
      std::vector<QVector3D> owned_targets;
      owned_units.reserve(command.units.size());
      owned_targets.reserve(command.units.size());

      for (std::size_t idx = 0; idx < command.units.size(); ++idx) {
        auto entity_id = command.units[idx];
        auto* entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto* unit = entity->get_component<Engine::Core::UnitComponent>();
        if ((unit == nullptr) || unit->owner_id != ai_owner_id) {
          continue;
        }

        owned_units.push_back(entity_id);
        owned_targets.emplace_back(expanded_x[idx], expanded_y[idx], expanded_z[idx]);
      }

      if (owned_units.empty()) {
        break;
      }

      CommandService::MoveOptions opts;
      opts.kind = MoveOrderKind::ScriptedMove;
      opts.allow_direct_fallback = true;
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
        auto* entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto* unit = entity->get_component<Engine::Core::UnitComponent>();
        if ((unit == nullptr) || unit->owner_id != ai_owner_id) {
          continue;
        }

        owned_units.push_back(entity_id);
      }

      if (owned_units.empty()) {
        break;
      }

      CommandService::attack_target(
          world, owned_units, command.target_id, command.should_chase);
      break;
    }

    case AICommandType::StartProduction: {
      auto* entity = world.get_entity(command.building_id);
      if (entity == nullptr) {
        break;
      }

      auto* production = entity->get_component<Engine::Core::ProductionComponent>();
      if (production == nullptr) {
        break;
      }

      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if ((unit != nullptr) && unit->owner_id != ai_owner_id) {
        break;
      }

      int const current_troops =
          Engine::Core::World::count_troops_for_player(ai_owner_id);
      int const max_troops = Game::GameConfig::instance().get_max_troops_per_player();
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(
              command.product_type);
      if (current_troops + production_cost > max_troops) {
        break;
      }
      if (production->manpower_available < production_cost) {
        break;
      }

      const std::vector<Engine::Core::EntityID> selected{command.building_id};
      const auto result = Game::Systems::ProductionService::
          start_production_for_first_selected_barracks(
              world, selected, ai_owner_id, command.product_type);
      if (result != Game::Systems::ProductionResult::Success) {
        break;
      }

      break;
    }

    case AICommandType::TriggerCommanderRally: {
      for (auto entity_id : command.units) {
        auto* entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }
        auto* unit = entity->get_component<Engine::Core::UnitComponent>();
        if ((unit == nullptr) || unit->owner_id != ai_owner_id) {
          continue;
        }
        auto* commander = entity->get_component<Engine::Core::CommanderComponent>();
        if (commander != nullptr) {
          commander->rally_requested = true;
        }
      }
      break;
    }

    case AICommandType::StartBuilderConstruction: {
      if (command.units.empty() || command.construction_type == nullptr) {
        break;
      }

      const auto resource_costs =
          Game::Systems::construction_cost_info(command.construction_type)
              .resource_costs;
      if (!resource_costs.empty() &&
          !Game::Systems::PlayerResourceRegistry::instance().has_at_least(
              ai_owner_id, resource_costs)) {
        break;
      }

      bool assigned_any = false;
      for (auto entity_id : command.units) {
        auto* entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }

        auto* unit = entity->get_component<Engine::Core::UnitComponent>();
        if ((unit == nullptr) || unit->owner_id != ai_owner_id) {
          continue;
        }

        if (unit->spawn_type != Game::Units::SpawnType::Builder) {
          continue;
        }

        auto* builder_prod =
            entity->get_component<Engine::Core::BuilderProductionComponent>();
        if (builder_prod == nullptr) {
          continue;
        }

        const std::string_view ctype(command.construction_type);
        builder_prod->product_type = command.construction_type;
        builder_prod->has_construction_site = true;
        builder_prod->construction_site_x = command.construction_site_x;
        builder_prod->construction_site_z = command.construction_site_z;
        builder_prod->at_construction_site = false;
        builder_prod->in_progress = false;
        builder_prod->construction_complete = false;
        builder_prod->is_placement_preview = false;

        if (ctype == BUILDING_TYPE_HOME) {
          builder_prod->build_time = BUILD_TIME_HOME;
        } else if (ctype == BUILDING_TYPE_DEFENSE_TOWER) {
          builder_prod->build_time = BUILD_TIME_DEFENSE_TOWER;
        } else if (ctype == BUILDING_TYPE_BARRACKS) {
          builder_prod->build_time = BUILD_TIME_BARRACKS;
        } else if (ctype == BUILDING_TYPE_MARKETPLACE) {
          builder_prod->build_time = BUILD_TIME_MARKETPLACE;
        } else {
          builder_prod->build_time = BUILD_TIME_DEFAULT;
        }
        builder_prod->time_remaining = builder_prod->build_time;
        assigned_any = true;
      }

      if (assigned_any) {
        Game::Systems::PlayerResourceRegistry::instance().spend(ai_owner_id,
                                                                resource_costs);
      }

      break;
    }
    }
  }
}

} // namespace Game::Systems::AI
