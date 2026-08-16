#include "order_feedback.h"

#include <QCoreApplication>

namespace App::Core {

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
    return no_eligible_units_reason(kind);
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

auto no_selection_reason() -> QString {
  return QCoreApplication::translate("OrderFeedback", "No units selected.");
}

auto no_eligible_units_reason(OrderKind kind) -> QString {
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

auto no_target_under_cursor_reason(OrderKind kind) -> QString {
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

auto no_ground_under_cursor_reason() -> QString {
  return QCoreApplication::translate("OrderFeedback", "Choose a spot on the map.");
}

auto barracks_full_reason() -> QString {
  return QCoreApplication::translate("OrderFeedback",
                                     "That barracks has no room for more people.");
}

auto no_repairs_needed_reason() -> QString {
  return QCoreApplication::translate("OrderFeedback",
                                     "That building does not need repairs.");
}

} // namespace App::Core
