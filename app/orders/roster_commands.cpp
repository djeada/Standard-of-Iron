#include "app/orders/roster_commands.h"

#include <QCoreApplication>

#include <algorithm>
#include <vector>

#include "app/orders/local_command.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/economy/nation_recruitment.h"
#include "game/systems/economy/production_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_queries.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/squad_service.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"

namespace App::Controllers {

namespace {

using Engine::Core::EntityID;

auto divisible_selection(Engine::Core::World& world,
                         const std::vector<EntityID>& selected)
    -> std::vector<EntityID> {
  std::vector<EntityID> divisible;
  for (const auto id : selected) {
    if (Game::Systems::SquadService::can_divide(world, id)) {
      divisible.push_back(id);
    }
  }
  return divisible;
}

auto mergeable_selection(Engine::Core::World& world,
                         const std::vector<EntityID>& selected)
    -> std::vector<EntityID> {
  std::vector<EntityID> mergeable;
  for (const auto& plan : Game::Systems::SquadService::plan_joins(world, selected)) {
    mergeable.insert(mergeable.end(), plan.members.begin(), plan.members.end());
  }
  return mergeable;
}

auto recruiting_building(Engine::Core::World& world,
                         const std::vector<EntityID>& selected,
                         Game::Units::TroopType product,
                         int local_owner_id,
                         std::optional<Game::Systems::NationID> band = std::nullopt)
    -> EntityID {
  using Game::Systems::ProductionService;
  const auto building =
      Game::Systems::quote_recruit(world, local_owner_id, product, band).building;
  switch (building) {
  case Game::Units::SpawnType::Home:
    return ProductionService::find_selected_home(world, selected, local_owner_id);
  case Game::Units::SpawnType::Temple:
    return ProductionService::find_selected_temple(world, selected, local_owner_id);
  case Game::Units::SpawnType::MercenaryCamp:
  case Game::Units::SpawnType::ElephantStables:
    for (const auto id : selected) {
      const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
      if (unit != nullptr && unit->owner_id == local_owner_id &&
          unit->spawn_type == building) {
        return id;
      }
    }
    return Engine::Core::NULL_ENTITY;
  default:
    return ProductionService::find_selected_barracks(world, selected, local_owner_id);
  }
}

// A recruit card names a mercenary band as "<nation>/<troop>", for example
// "gauls/swordsman" (NATION_ASYMMETRY.md 4.2); a plain troop name is the
// owner's own.
auto parse_recruit_key(const QString& key)
    -> std::pair<Game::Units::TroopType, std::optional<Game::Systems::NationID>> {
  const auto slash = key.indexOf(QLatin1Char('/'));
  if (slash < 0) {
    return {Game::Units::troop_typeFromString(key.toStdString()), std::nullopt};
  }
  return {Game::Units::troop_typeFromString(key.mid(slash + 1).toStdString()),
          Game::Systems::nation_id_from_string(key.left(slash).toStdString())};
}

auto missing_resources(const Game::Systems::ResourceAmounts& cost,
                       const Game::Systems::ResourceAmounts& have)
    -> Game::Systems::ResourceAmounts {
  Game::Systems::ResourceAmounts missing;
  for (const auto type : Game::Systems::k_all_resource_types) {
    missing.set(type, std::max(0, cost.get(type) - have.get(type)));
  }
  return missing;
}

auto recruit_refusal(Engine::Core::World& world,
                     Game::Systems::ProductionResult ruling,
                     EntityID building,
                     Game::Units::TroopType product,
                     int local_owner_id,
                     std::optional<Game::Systems::NationID> band = std::nullopt)
    -> App::Core::OrderRefusal {
  auto& session = Game::Session::session_for(world);
  const auto* nation = session.nations().get_nation_for_player(local_owner_id);
  const auto& profile = Game::Systems::TroopProfileService::instance().get_profile_ref(
      nation != nullptr ? nation->id : session.nations().default_nation_id(), product);
  switch (ruling) {
  case Game::Systems::ProductionResult::InsufficientManpower: {
    const auto* production = world.try_get<Engine::Core::ProductionComponent>(building);
    return App::Core::reserve_short_reason(
        production != nullptr ? production->manpower_available : 0,
        profile.production.cost);
  }
  case Game::Systems::ProductionResult::GlobalTroopLimitReached:
    return App::Core::army_cap_reason(
        Game::Systems::troop_count_for(world, local_owner_id),
        Game::GameConfig::instance().get_max_troops_per_player());
  case Game::Systems::ProductionResult::InsufficientResources:
    return App::Core::missing_resources_reason(missing_resources(
        Game::Systems::quote_recruit(world, local_owner_id, product, band).resources,
        session.economy().get_all(local_owner_id)));
  case Game::Systems::ProductionResult::NotInRoster:
    return App::Core::no_eligible_units_reason(App::Core::OrderKind::Recruit);
  case Game::Systems::ProductionResult::NeedsBuilding: {
    auto refusal = App::Core::no_eligible_units_reason(App::Core::OrderKind::Recruit);
    const auto needs =
        Game::Systems::quote_recruit(world, local_owner_id, product, band)
            .requires_building;
    refusal.text =
        needs == Game::Units::SpawnType::Fabrica
            ? QCoreApplication::translate(
                  "RosterCommands", "Triarii are armed at a Fabrica. Build one first.")
            : QCoreApplication::translate("RosterCommands",
                                          "This troop needs another building first.");
    return refusal;
  }
  case Game::Systems::ProductionResult::TypeLimitReached: {
    auto refusal = App::Core::barracks_full_reason();
    refusal.text =
        QCoreApplication::translate("RosterCommands",
                                    "You already field the most of these: %1 squads.")
            .arg(Game::Systems::quote_recruit(world, local_owner_id, product, band)
                     .max_squads);
    return refusal;
  }
  case Game::Systems::ProductionResult::QueueFull:
  case Game::Systems::ProductionResult::AlreadyInProgress:
    return App::Core::training_queue_full_reason();
  case Game::Systems::ProductionResult::PerBarracksLimitReached:
    return App::Core::barracks_full_reason();
  case Game::Systems::ProductionResult::NoBarracks:
  case Game::Systems::ProductionResult::WrongBuilding:
    return App::Core::no_eligible_units_reason(App::Core::OrderKind::Recruit);
  case Game::Systems::ProductionResult::CommanderNotRecruitable:
    return App::Core::rejection_refusal(Game::Command::Rejection::NotPermittedForSource,
                                        App::Core::OrderKind::Recruit);
  case Game::Systems::ProductionResult::Success:
    break;
  }
  return App::Core::rejection_refusal(Game::Command::Rejection::MalformedPayload,
                                      App::Core::OrderKind::Recruit);
}

auto batch_shortfall_text(int queued,
                          int wanted,
                          Game::Systems::ProductionResult limit) -> QString {
  using Game::Systems::ProductionResult;
  QString why;
  switch (limit) {
  case ProductionResult::InsufficientManpower:
    why = QCoreApplication::translate("RosterCommands", "the reserve ran out");
    break;
  case ProductionResult::InsufficientResources:
    why = QCoreApplication::translate("RosterCommands", "the stores ran short");
    break;
  case ProductionResult::GlobalTroopLimitReached:
    why = QCoreApplication::translate("RosterCommands", "the army is at its cap");
    break;
  case ProductionResult::PerBarracksLimitReached:
    why = QCoreApplication::translate("RosterCommands", "the building is full");
    break;
  default:
    why = QCoreApplication::translate("RosterCommands", "the queue is full");
    break;
  }
  return QCoreApplication::translate("RosterCommands", "Queued %1 of %2 recruits: %3.")
      .arg(queued)
      .arg(wanted)
      .arg(why);
}

} // namespace

RosterCommands::RosterCommands(Engine::Core::World* world,
                               Game::Session::SelectionService* selection,
                               App::Orders::OrderIssuer& orders)
    : m_world(world)
    , m_selection(selection)
    , m_orders(orders) {
}

void RosterCommands::recruit_near_selected(const QString& unit_type,
                                           int local_owner_id,
                                           RecruitClick click) {
  if ((m_world == nullptr) || (m_selection == nullptr)) {
    return;
  }

  const auto& sel = m_selection->get_selected_units();
  if (sel.empty()) {
    return;
  }

  const auto [product, band] = parse_recruit_key(unit_type);
  const EntityID building =
      recruiting_building(*m_world, sel, product, local_owner_id, band);
  if (building == Engine::Core::NULL_ENTITY) {
    return;
  }

  if (band.has_value()) {
    // A band is one contract at a time: no batches, no repeat.
    const auto ruling = Game::Systems::ProductionService::can_start_production(
        *m_world, building, product, band);
    if (ruling != Game::Systems::ProductionResult::Success) {
      (void)m_orders.reject_on(
          App::Core::OrderKind::Recruit,
          recruit_refusal(*m_world, ruling, building, product, local_owner_id, band),
          building);
      return;
    }
    App::Orders::submit_local_command(m_world,
                                      Game::Command::Produce{.building = building,
                                                             .product = product,
                                                             .band_nation = band});
    return;
  }

  if (click == RecruitClick::Repeat) {
    App::Orders::submit_local_command(m_world,
                                      Game::Command::Produce{.building = building,
                                                             .product = product,
                                                             .toggle_repeat = true});
    return;
  }

  const int wanted = click == RecruitClick::Batch
                         ? Game::Systems::ProductionService::k_recruit_batch
                         : 1;
  const auto plan =
      Game::Systems::ProductionService::plan_batch(*m_world, building, product, wanted);
  if (plan.count <= 0) {
    (void)m_orders.reject_on(
        App::Core::OrderKind::Recruit,
        recruit_refusal(*m_world, plan.limit, building, product, local_owner_id),
        building);
    return;
  }
  App::Orders::submit_local_command(m_world,
                                    Game::Command::Produce{.building = building,
                                                           .product = product,
                                                           .count = plan.count});
  if (plan.count < wanted) {
    auto refusal =
        recruit_refusal(*m_world, plan.limit, building, product, local_owner_id);
    refusal.text = batch_shortfall_text(plan.count, wanted, plan.limit);
    (void)m_orders.reject_on(
        App::Core::OrderKind::Recruit, std::move(refusal), building);
  }
}

auto RosterCommands::divide_selected_squads() -> CommandResult {
  CommandResult result;
  if ((m_selection == nullptr) || (m_world == nullptr)) {
    return result;
  }
  const auto selected = m_selection->get_selected_units();
  const auto divisible = divisible_selection(*m_world, selected);
  if (divisible.empty()) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Squad,
        selected.empty()
            ? App::Core::no_selection_reason()
            : App::Core::no_eligible_units_reason(App::Core::OrderKind::Squad));
    return result;
  }

  result.order = m_orders.issue(App::Core::OrderKind::Squad,
                                Game::Command::DivideSquads{.units = divisible});
  result.input_consumed = result.order.accepted();
  return result;
}

auto RosterCommands::merge_selected_squads() -> CommandResult {
  CommandResult result;
  if ((m_selection == nullptr) || (m_world == nullptr)) {
    return result;
  }
  const auto selected = m_selection->get_selected_units();
  const auto mergeable = mergeable_selection(*m_world, selected);
  if (mergeable.size() < 2U) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Squad,
        selected.empty()
            ? App::Core::no_selection_reason()
            : App::Core::no_eligible_units_reason(App::Core::OrderKind::Squad));
    return result;
  }

  result.order = m_orders.issue(App::Core::OrderKind::Squad,
                                Game::Command::MergeSquads{.units = mergeable});
  result.input_consumed = result.order.accepted();
  return result;
}

} // namespace App::Controllers
