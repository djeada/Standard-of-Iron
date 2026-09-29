#include "app/core/order_feedback_presenter.h"

#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "app/mission/tutorial_runtime.h"
#include "app/orders/action_vfx.h"
#include "app/orders/command_controller.h"
#include "app/viewmodels/minimap_view_model.h"
#include "game/audio/audio_cues.h"
#include "game/core/event_manager.h"
#include "game/render_bridge/selection_controller.h"

namespace App::Core {

namespace {

auto accepted_order_cue(OrderKind kind,
                        const Game::Audio::Cue::SelectionMounts& mounts) -> const
    char* {
  switch (kind) {
  case OrderKind::Move:
    return App::Controllers::CommandController::move_order_cue(mounts);
  case OrderKind::Deliver:
  case OrderKind::Repair:
    return Game::Audio::Cue::k_order_move;
  case OrderKind::Attack:
    return Game::Audio::Cue::k_order_attack;
  case OrderKind::Patrol:
    return Game::Audio::Cue::k_order_patrol;
  case OrderKind::Stop:
    return Game::Audio::Cue::k_order_stop;
  case OrderKind::Rally:
    return Game::Audio::Cue::k_order_rally_set;
  case OrderKind::Build:
    return Game::Audio::Cue::k_build_placement_confirmed;
  case OrderKind::Gather:
    return Game::Audio::Cue::k_order_move;
  case OrderKind::Guard:
    return Game::Audio::Cue::k_order_guard;
  case OrderKind::Hold:
    return Game::Audio::Cue::k_order_hold;
  case OrderKind::Formation:
    return Game::Audio::Cue::k_order_formation_placed;
  case OrderKind::Squad:
  case OrderKind::Recruit:
  case OrderKind::None:
    break;
  }
  return nullptr;
}

auto player_feedback_event_for(const OrderOutcome& outcome) -> PlayerFeedbackEvent {
  PlayerFeedbackEvent event;
  event.type = outcome.accepted() ? PlayerFeedbackType::OrderIssued
                                  : PlayerFeedbackType::OrderRejected;
  if (!outcome.accepted() && outcome.failure == OrderFailure::InsufficientResources) {
    event.type = PlayerFeedbackType::ResourceInsufficient;
  }
  event.entity = outcome.target;
  event.has_world_position = outcome.has_destination;
  event.world_position = outcome.destination;
  event.reason = outcome.accepted()
                     ? QString::fromLatin1(order_kind_name(outcome.kind))
                     : QString::fromLatin1(order_failure_name(outcome.failure));
  return event;
}

} // namespace

auto OrderFeedbackPresenter::late_rejection(const Game::Command::Command& command,
                                            Game::Command::Rejection reason)
    -> std::optional<OrderOutcome> {
  if (command.source != Game::Command::Source::LocalPlayer) {
    return std::nullopt;
  }

  const auto kind = std::visit(
      [](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, Game::Command::AttackTarget>) {
          return OrderKind::Attack;
        } else if constexpr (std::is_same_v<T, Game::Command::Move>) {
          return OrderKind::Move;
        } else {
          return OrderKind::None;
        }
      },
      command.payload);

  OrderOutcome outcome;
  outcome.kind = kind;
  outcome.status = OrderStatus::Rejected;
  outcome.rejection = reason;
  outcome.failure = failure_for(reason);
  outcome.reason = rejection_reason_text(reason, kind);
  return outcome;
}

auto OrderFeedbackPresenter::present(const OrderOutcome& outcome)
    -> std::optional<OrderAnnouncement> {
  if (!outcome.issued()) {
    return std::nullopt;
  }

  if (const auto* marker = m_markers.push(outcome, m_sources.world)) {
    m_sources.minimap->note_order_marker(*marker);
  }

  m_sources.feedback->publish(player_feedback_event_for(outcome));
  m_sources.tutorial->note_order_outcome(outcome);

  return OrderAnnouncement{
      .kind = QString::fromLatin1(order_kind_name(outcome.kind)),
      .accepted = outcome.accepted(),
      .message =
          outcome.accepted() ? present_accepted(outcome) : present_refused(outcome),
      .failure = QString::fromLatin1(order_failure_name(outcome.failure))};
}

auto OrderFeedbackPresenter::present_accepted(const OrderOutcome& outcome) const
    -> QString {
  Game::Audio::Cue::SelectionMounts mounts;
  if (outcome.kind == OrderKind::Move && m_sources.world != nullptr) {
    std::vector<Engine::Core::EntityID> selected;
    m_sources.selection->get_selected_unit_ids(selected);
    mounts = App::Controllers::CommandController::selection_mounts(*m_sources.world,
                                                                   selected);
  }
  if (const char* cue = accepted_order_cue(outcome.kind, mounts)) {
    Game::Audio::play_cue(cue);
  } else if (outcome.kind != OrderKind::Recruit) {

    Game::Audio::play_cue(Game::Audio::Cue::k_command_accept);
  }
  if (outcome.kind == OrderKind::Attack && outcome.target != 0) {
    App::Controllers::ActionVFX::spawn_attack_arrow(m_sources.world, outcome.target);
  }
  return accepted_order_message(outcome);
}

auto OrderFeedbackPresenter::present_refused(const OrderOutcome& outcome) const
    -> QString {
  if (outcome.kind == OrderKind::Recruit) {
    warn(outcome.failure == OrderFailure::PopulationCap
             ? Game::Audio::Cue::k_alert_population_limit
             : Game::Audio::Cue::k_alert_low_resources);
  } else {
    Game::Audio::play_cue(Game::Audio::Cue::k_command_refuse);
  }
  return outcome.reason;
}

void OrderFeedbackPresenter::warn(const char* cue_id) const {
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent::for_owner(*m_sources.local_owner_id, cue_id));
}

} // namespace App::Core
