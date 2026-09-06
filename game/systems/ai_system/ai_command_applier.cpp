#include "ai_command_applier.h"

#include <QDebug>
#include <QVector3D>
#include <qvectornd.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "../../command/command.h"
#include "../../command/command_dispatcher.h"
#include "../../command/command_queue.h"
#include "../../core/component_structures.h"
#include "../../core/world.h"
#include "../../game_config.h"
#include "../../map/terrain_service.h"
#include "../../session/session_context.h"
#include "../../units/troop_config.h"
#include "../build_site.h"
#include "../building_collision_registry.h"
#include "../combat_rules.h"
#include "../combat_system/combat_utils.h"
#include "../command_service.h"
#include "../construction_cost_catalog.h"
#include "../nation_registry.h"
#include "../owner_queries.h"
#include "../player_resource_registry.h"
#include "../production_service.h"
#include "../troop_profile_service.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {

void submit(Engine::Core::World& world, int owner_id, Game::Command::Payload payload) {
  Game::Command::submit(world, Game::Command::Source::AI, owner_id, std::move(payload));
}

[[nodiscard]] auto production_refusal_name(ProductionResult result) -> const char* {
  switch (result) {
  case ProductionResult::Success:
    return "success";
  case ProductionResult::NoBarracks:
    return "no_barracks";
  case ProductionResult::InsufficientManpower:
    return "insufficient_manpower";
  case ProductionResult::InsufficientResources:
    return "insufficient_resources";
  case ProductionResult::PerBarracksLimitReached:
    return "per_barracks_limit";
  case ProductionResult::WrongBuilding:
    return "wrong_building";
  case ProductionResult::GlobalTroopLimitReached:
    return "global_troop_limit";
  case ProductionResult::CommanderNotRecruitable:
    return "commander_not_recruitable";
  case ProductionResult::AlreadyInProgress:
    return "already_in_progress";
  case ProductionResult::QueueFull:
    return "queue_full";
  }
  return "unknown";
}

void trace_refused_production(int owner_id,
                              Game::Units::TroopType product,
                              ProductionResult ruling,
                              const Engine::Core::ProductionComponent& production,
                              int cost,
                              int population,
                              int population_cap,
                              const Game::Systems::ResourceAmounts& need,
                              const Game::Systems::ResourceAmounts& have) {
  static const bool enabled = !qEnvironmentVariableIsEmpty("SOI_AI_TRACE");
  if (!enabled) {
    return;
  }
  qInfo().nospace() << "SOI_AI_TRACE refused player=" << owner_id << " product="
                    << QString::fromStdString(Game::Units::troop_typeToString(product))
                    << " reason=" << production_refusal_name(ruling) << " cost=" << cost
                    << " reserve=" << production.manpower_available
                    << " produced=" << production.produced_count
                    << " max_units=" << production.max_units
                    << " queue=" << production.production_queue.size()
                    << " in_progress=" << production.in_progress
                    << " population=" << population << "/" << population_cap;
  for (const auto type : Game::Systems::k_all_resource_types) {
    if (need.get(type) > 0) {
      qInfo().nospace() << "   needs " << Game::Systems::resource_type_key(type) << " "
                        << need.get(type) << " has " << have.get(type);
    }
  }
}
} // namespace

auto AICommandApplier::apply(Engine::Core::World& world,
                             int ai_owner_id,
                             const std::vector<AICommand>& commands) -> ApplyReport {
  ApplyReport report;

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
        auto* attacker = world.get_entity(unit_id);
        if (!Game::Systems::Combat::melee_walled_off_from(attacker, target)) {
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
      if (const auto ruling = Game::Systems::ProductionService::can_start_production(
              world, command.building_id, command.product_type);
          ruling != Game::Systems::ProductionResult::Success) {
        ++report.refused_production;
        const auto* owner_nation =
            Game::Session::session_for(world).nations().get_nation_for_player(
                ai_owner_id);
        const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
            owner_nation != nullptr ? owner_nation->id
                                    : Game::Systems::NationID::RomanRepublic,
            command.product_type);
        trace_refused_production(
            ai_owner_id,
            command.product_type,
            ruling,
            *production,
            profile.production.cost,
            Game::Systems::troop_count_for(world, ai_owner_id),
            Game::GameConfig::instance().get_max_troops_per_player(),
            profile.production.resource_costs,
            Game::Session::session_for(world).economy().get_all(ai_owner_id));
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::Produce{.building = command.building_id,
                                    .product = command.product_type});
      break;
    }

    case AICommandType::DeliverCivilians: {
      if (command.units.empty() || command.building_id == 0) {
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::DeliverCivilians{.units = command.units,
                                             .barracks = command.building_id});
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

      constexpr float k_site_nudge_radius = 12.0F;

      constexpr float k_wall_link_nudge_radius = 1.0F;
      const bool wall_link =
          Game::Systems::is_wall_link_building_type(command.construction_type);
      const auto site = Game::Systems::find_clear_site(
          world,
          command.construction_type,
          QVector3D(command.construction_site_x, 0.0F, command.construction_site_z),
          wall_link ? k_wall_link_nudge_radius : k_site_nudge_radius,
          command.construction_rotation_y,
          command.units);
      if (!site.has_value()) {

        ++report.refused_construction;
        if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
          qWarning() << "BUILDTRACE p" << ai_owner_id << "no legal site for"
                     << command.construction_type << "near"
                     << command.construction_site_x << command.construction_site_z
                     << "verdict"
                     << static_cast<int>(
                            Game::Systems::assess_ground(world,
                                                         command.construction_type,
                                                         command.construction_site_x,
                                                         command.construction_site_z));
        }
        break;
      }
      if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
        qWarning() << "BUILDTRACE p" << ai_owner_id << "submits"
                   << command.construction_type << "at" << site->x() << site->z();
      }
      submit(world,
             ai_owner_id,
             Game::Command::StartConstruction{
                 .units = command.units,
                 .construction_type = command.construction_type,
                 .site = *site,
                 .rotation_y = command.construction_rotation_y});
      break;
    }

    case AICommandType::StartBuilderRepair: {
      if (command.units.empty() || command.target_id == 0) {
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::RepairStructure{.units = command.units,
                                            .structure = command.target_id});
      break;
    }

    case AICommandType::DivideSquads: {
      if (command.units.empty()) {
        break;
      }
      submit(world, ai_owner_id, Game::Command::DivideSquads{.units = command.units});
      break;
    }

    case AICommandType::MergeSquads: {
      if (command.units.size() < 2U) {
        break;
      }
      submit(world, ai_owner_id, Game::Command::MergeSquads{.units = command.units});
      break;
    }

    case AICommandType::SetAutoGather: {
      if (command.units.empty()) {
        break;
      }
      submit(world,
             ai_owner_id,
             Game::Command::SetAutoGather{
                 .units = command.units,
                 .active = command.auto_gather_active,
                 .priority_product_type = command.construction_type != nullptr
                                              ? std::string(command.construction_type)
                                              : std::string()});
      break;
    }

    case AICommandType::TradeResource: {
      submit(
          world,
          ai_owner_id,
          Game::Command::Trade{.resource = command.trade_resource,
                               .direction = command.trade_is_purchase
                                                ? Game::Command::TradeDirection::Buy
                                                : Game::Command::TradeDirection::Sell});
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

  return report;
}

} // namespace Game::Systems::AI
