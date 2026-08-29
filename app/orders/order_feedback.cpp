#include "app/orders/order_feedback.h"

#include <QCoreApplication>

namespace App::Core {
namespace {

auto no_eligible_units_text(OrderKind kind) -> QString;
auto no_target_under_cursor_text(OrderKind kind) -> QString;

} // namespace

auto order_failure_name(OrderFailure failure) -> const char* {
  switch (failure) {
  case OrderFailure::None:
    return "none";
  case OrderFailure::NoSelection:
    return "no_selection";
  case OrderFailure::InvalidTarget:
    return "invalid_target";
  case OrderFailure::Unreachable:
    return "unreachable";
  case OrderFailure::WrongOwner:
    return "wrong_owner";
  case OrderFailure::OutOfRange:
    return "out_of_range";
  case OrderFailure::InsufficientResources:
    return "insufficient_resources";
  case OrderFailure::PopulationCap:
    return "population_cap";
  case OrderFailure::UnitBusy:
    return "unit_busy";
  case OrderFailure::CommandUnavailable:
    return "command_unavailable";
  }
  return "command_unavailable";
}

auto failure_for(Game::Command::Rejection rejection) -> OrderFailure {
  switch (rejection) {
  case Game::Command::Rejection::None:
    return OrderFailure::None;
  case Game::Command::Rejection::NoOwner:
    return OrderFailure::WrongOwner;
  case Game::Command::Rejection::NoSubjects:
    return OrderFailure::NoSelection;
  case Game::Command::Rejection::DeadTarget:
  case Game::Command::Rejection::FriendlyTarget:
  case Game::Command::Rejection::MissingBuilding:
    return OrderFailure::InvalidTarget;
  case Game::Command::Rejection::NotOwnedBuilding:
    return OrderFailure::WrongOwner;
  case Game::Command::Rejection::NotPermittedForSource:
  case Game::Command::Rejection::MalformedPayload:
    return OrderFailure::CommandUnavailable;
  }
  return OrderFailure::CommandUnavailable;
}

auto rejection_refusal(Game::Command::Rejection rejection,
                       OrderKind kind) -> OrderRefusal {
  return {failure_for(rejection), rejection_reason_text(rejection, kind)};
}

auto order_kind_name(OrderKind kind) -> const char* {
  switch (kind) {
  case OrderKind::None:
    return "none";
  case OrderKind::Move:
    return "move";
  case OrderKind::Attack:
    return "attack";
  case OrderKind::Guard:
    return "guard";
  case OrderKind::Patrol:
    return "patrol";
  case OrderKind::Hold:
    return "hold";
  case OrderKind::Stop:
    return "stop";
  case OrderKind::Build:
    return "build";
  case OrderKind::Gather:
    return "gather";
  case OrderKind::Deliver:
    return "deliver";
  case OrderKind::Repair:
    return "repair";
  case OrderKind::Rally:
    return "rally";
  case OrderKind::Formation:
    return "formation";
  case OrderKind::Squad:
    return "squad";
  }
  return "unknown";
}

auto order_kind_display_name(OrderKind kind) -> QString {
  switch (kind) {
  case OrderKind::None:
    return {};
  case OrderKind::Move:
    return QCoreApplication::translate("OrderFeedback", "Move");
  case OrderKind::Attack:
    return QCoreApplication::translate("OrderFeedback", "Attack");
  case OrderKind::Guard:
    return QCoreApplication::translate("OrderFeedback", "Guard");
  case OrderKind::Patrol:
    return QCoreApplication::translate("OrderFeedback", "Patrol");
  case OrderKind::Hold:
    return QCoreApplication::translate("OrderFeedback", "Hold position");
  case OrderKind::Stop:
    return QCoreApplication::translate("OrderFeedback", "Stop");
  case OrderKind::Build:
    return QCoreApplication::translate("OrderFeedback", "Build");
  case OrderKind::Gather:
    return QCoreApplication::translate("OrderFeedback", "Gather");
  case OrderKind::Deliver:
    return QCoreApplication::translate("OrderFeedback", "Deliver");
  case OrderKind::Repair:
    return QCoreApplication::translate("OrderFeedback", "Repair");
  case OrderKind::Rally:
    return QCoreApplication::translate("OrderFeedback", "Rally point");
  case OrderKind::Formation:
    return QCoreApplication::translate("OrderFeedback", "Formation");
  case OrderKind::Squad:
    return QCoreApplication::translate("OrderFeedback", "Squad");
  }
  return {};
}

auto rejection_reason_text(Game::Command::Rejection rejection,
                           OrderKind kind) -> QString {
  switch (rejection) {
  case Game::Command::Rejection::None:
    return {};
  case Game::Command::Rejection::NoOwner:
    return QCoreApplication::translate("OrderFeedback",
                                       "You are not in command of these units.");
  case Game::Command::Rejection::NoSubjects:
    return no_eligible_units_text(kind);
  case Game::Command::Rejection::DeadTarget:
    return QCoreApplication::translate("OrderFeedback", "That target is already gone.");
  case Game::Command::Rejection::FriendlyTarget:
    return QCoreApplication::translate("OrderFeedback",
                                       "Cannot attack a friendly or neutral target.");
  case Game::Command::Rejection::MissingBuilding:
    return QCoreApplication::translate("OrderFeedback",
                                       "That building no longer exists.");
  case Game::Command::Rejection::NotOwnedBuilding:
    return QCoreApplication::translate("OrderFeedback", "That building is not yours.");
  case Game::Command::Rejection::NotPermittedForSource:
    return QCoreApplication::translate("OrderFeedback",
                                       "That order is not available here.");
  case Game::Command::Rejection::MalformedPayload:
    return QCoreApplication::translate("OrderFeedback",
                                       "That order could not be carried out.");
  }
  return QCoreApplication::translate("OrderFeedback",
                                     "That order could not be carried out.");
}

auto accepted_order_message(const OrderOutcome& outcome) -> QString {
  const QString name = order_kind_display_name(outcome.kind);
  if (name.isEmpty()) {
    return {};
  }
  if (outcome.unit_count <= 1) {
    return name;
  }
  return QCoreApplication::translate("OrderFeedback", "%1: %2 units")
      .arg(name)
      .arg(static_cast<qulonglong>(outcome.unit_count));
}

auto no_selection_reason() -> OrderRefusal {
  return {OrderFailure::NoSelection,
          QCoreApplication::translate("OrderFeedback", "No units selected.")};
}

auto unreachable_reason() -> OrderRefusal {
  return {OrderFailure::Unreachable,
          QCoreApplication::translate("OrderFeedback", "Cannot reach that spot.")};
}

auto out_of_range_reason() -> OrderRefusal {
  return {OrderFailure::OutOfRange,
          QCoreApplication::translate("OrderFeedback", "That target is out of range.")};
}

auto unit_busy_reason() -> OrderRefusal {
  return {OrderFailure::UnitBusy,
          QCoreApplication::translate("OrderFeedback",
                                      "Those units are busy with another order.")};
}

auto hauling_load_reason() -> OrderRefusal {
  return {OrderFailure::UnitBusy,
          QCoreApplication::translate(
              "OrderFeedback",
              "Hauling a load - it cannot be interrupted until the load is "
              "dropped off.")};
}

auto insufficient_resources_reason() -> OrderRefusal {
  return {OrderFailure::InsufficientResources,
          QCoreApplication::translate("OrderFeedback", "Not enough resources.")};
}

namespace {

auto no_eligible_units_text(OrderKind kind) -> QString {
  switch (kind) {
  case OrderKind::Attack:
    return QCoreApplication::translate("OrderFeedback",
                                       "The selected units cannot attack.");
  case OrderKind::Guard:
    return QCoreApplication::translate("OrderFeedback",
                                       "The selected units cannot guard.");
  case OrderKind::Patrol:
    return QCoreApplication::translate("OrderFeedback",
                                       "The selected units cannot patrol.");
  case OrderKind::Hold:
    return QCoreApplication::translate("OrderFeedback",
                                       "The selected units cannot hold position.");
  case OrderKind::Build:
    return QCoreApplication::translate("OrderFeedback",
                                       "The selected units cannot build.");
  case OrderKind::Gather:
    return QCoreApplication::translate("OrderFeedback",
                                       "The selected units cannot gather.");
  case OrderKind::Deliver:
    return QCoreApplication::translate(
        "OrderFeedback", "Only civilians can be delivered to a barracks.");
  case OrderKind::Repair:
    return QCoreApplication::translate("OrderFeedback",
                                       "Only builders can repair structures.");
  case OrderKind::Squad:
    return QCoreApplication::translate(
        "OrderFeedback",
        "Only a squad of several individuals can be divided or joined.");
  case OrderKind::Move:
  case OrderKind::Stop:
  case OrderKind::Rally:
  case OrderKind::Formation:
  case OrderKind::None:
    break;
  }
  return QCoreApplication::translate("OrderFeedback",
                                     "The selected units cannot take that order.");
}

auto no_target_under_cursor_text(OrderKind kind) -> QString {
  switch (kind) {
  case OrderKind::Attack:
    return QCoreApplication::translate("OrderFeedback", "No enemy under the cursor.");
  case OrderKind::Deliver:
    return QCoreApplication::translate(
        "OrderFeedback", "Select a friendly barracks with room for more people.");
  case OrderKind::Repair:
    return QCoreApplication::translate("OrderFeedback",
                                       "Select a damaged friendly building to repair.");
  default:
    break;
  }
  return QCoreApplication::translate("OrderFeedback", "Nothing to target there.");
}

} // namespace

auto no_eligible_units_reason(OrderKind kind) -> OrderRefusal {
  return {OrderFailure::CommandUnavailable, no_eligible_units_text(kind)};
}

auto no_target_under_cursor_reason(OrderKind kind) -> OrderRefusal {
  return {OrderFailure::InvalidTarget, no_target_under_cursor_text(kind)};
}

auto no_ground_under_cursor_reason() -> OrderRefusal {
  return {OrderFailure::InvalidTarget,
          QCoreApplication::translate("OrderFeedback", "Choose a spot on the map.")};
}

auto barracks_full_reason() -> OrderRefusal {
  return {OrderFailure::PopulationCap,
          QCoreApplication::translate("OrderFeedback",
                                      "That barracks has no room for more people.")};
}

auto no_repairs_needed_reason() -> OrderRefusal {
  return {OrderFailure::CommandUnavailable,
          QCoreApplication::translate("OrderFeedback",
                                      "That building does not need repairs.")};
}

auto not_your_building_reason() -> OrderRefusal {
  return {OrderFailure::WrongOwner,
          QCoreApplication::translate(
              "OrderFeedback", "Your builders only take down your own buildings.")};
}

auto building_is_protected_reason() -> OrderRefusal {
  return {OrderFailure::CommandUnavailable,
          QCoreApplication::translate("OrderFeedback",
                                      "That building cannot be taken down.")};
}

} // namespace App::Core
