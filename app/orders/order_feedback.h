#pragma once

#include <QString>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "game/command/command_validator.h"

namespace App::Core {

enum class OrderKind : std::uint8_t {
  None,
  Move,
  Attack,
  Guard,
  Patrol,
  Hold,
  Stop,
  Build,
  Gather,
  Deliver,
  Repair,
  Rally,
  Formation,
};

enum class OrderStatus : std::uint8_t {
  NotIssued,
  Accepted,
  Rejected,
};

enum class OrderFailure : std::uint8_t {
  None,
  NoSelection,
  InvalidTarget,
  Unreachable,
  WrongOwner,
  OutOfRange,
  InsufficientResources,
  PopulationCap,
  UnitBusy,
  CommandUnavailable,
};

struct OrderRefusal {
  OrderFailure failure = OrderFailure::CommandUnavailable;
  QString text;
};

struct OrderOutcome {
  OrderKind kind = OrderKind::None;
  OrderStatus status = OrderStatus::NotIssued;

  Engine::Core::EntityID target = 0;

  bool has_destination = false;
  QVector3D destination;

  std::size_t unit_count = 0;

  Game::Command::Rejection rejection = Game::Command::Rejection::None;
  OrderFailure failure = OrderFailure::None;

  QString reason;

  [[nodiscard]] auto accepted() const -> bool {
    return status == OrderStatus::Accepted;
  }
  [[nodiscard]] auto rejected() const -> bool {
    return status == OrderStatus::Rejected;
  }
  [[nodiscard]] auto issued() const -> bool { return status != OrderStatus::NotIssued; }
};

[[nodiscard]] auto order_kind_name(OrderKind kind) -> const char*;

[[nodiscard]] auto order_failure_name(OrderFailure failure) -> const char*;

[[nodiscard]] auto failure_for(Game::Command::Rejection rejection) -> OrderFailure;

[[nodiscard]] auto order_kind_display_name(OrderKind kind) -> QString;

[[nodiscard]] auto rejection_reason_text(Game::Command::Rejection rejection,
                                         OrderKind kind) -> QString;

[[nodiscard]] auto accepted_order_message(const OrderOutcome& outcome) -> QString;

[[nodiscard]] auto rejection_refusal(Game::Command::Rejection rejection,
                                     OrderKind kind) -> OrderRefusal;

[[nodiscard]] auto no_selection_reason() -> OrderRefusal;
[[nodiscard]] auto no_eligible_units_reason(OrderKind kind) -> OrderRefusal;
[[nodiscard]] auto no_target_under_cursor_reason(OrderKind kind) -> OrderRefusal;
[[nodiscard]] auto no_ground_under_cursor_reason() -> OrderRefusal;
[[nodiscard]] auto unreachable_reason() -> OrderRefusal;
[[nodiscard]] auto out_of_range_reason() -> OrderRefusal;
[[nodiscard]] auto unit_busy_reason() -> OrderRefusal;
[[nodiscard]] auto insufficient_resources_reason() -> OrderRefusal;
[[nodiscard]] auto barracks_full_reason() -> OrderRefusal;
[[nodiscard]] auto no_repairs_needed_reason() -> OrderRefusal;
[[nodiscard]] auto not_your_building_reason() -> OrderRefusal;
[[nodiscard]] auto building_is_protected_reason() -> OrderRefusal;

} // namespace App::Core
