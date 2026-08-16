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

struct OrderOutcome {
  OrderKind kind = OrderKind::None;
  OrderStatus status = OrderStatus::NotIssued;

  Engine::Core::EntityID target = 0;

  bool has_destination = false;
  QVector3D destination;

  std::size_t unit_count = 0;

  Game::Command::Rejection rejection = Game::Command::Rejection::None;

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

[[nodiscard]] auto order_kind_display_name(OrderKind kind) -> QString;

[[nodiscard]] auto rejection_reason_text(Game::Command::Rejection rejection,
                                         OrderKind kind) -> QString;

[[nodiscard]] auto accepted_order_message(const OrderOutcome& outcome) -> QString;

[[nodiscard]] auto no_selection_reason() -> QString;
[[nodiscard]] auto no_eligible_units_reason(OrderKind kind) -> QString;
[[nodiscard]] auto no_target_under_cursor_reason(OrderKind kind) -> QString;
[[nodiscard]] auto no_ground_under_cursor_reason() -> QString;
[[nodiscard]] auto barracks_full_reason() -> QString;
[[nodiscard]] auto no_repairs_needed_reason() -> QString;

} // namespace App::Core
