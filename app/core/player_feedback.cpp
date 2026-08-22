#include "app/core/player_feedback.h"

#include <algorithm>
#include <utility>

namespace App::Core {

auto player_feedback_type_name(PlayerFeedbackType type) -> const char* {
  switch (type) {
  case PlayerFeedbackType::OrderIssued:
    return "order_issued";
  case PlayerFeedbackType::OrderRejected:
    return "order_rejected";
  case PlayerFeedbackType::SelectionChanged:
    return "selection_changed";
  case PlayerFeedbackType::AttackCommitted:
    return "attack_committed";
  case PlayerFeedbackType::WeaponContact:
    return "weapon_contact";
  case PlayerFeedbackType::PerfectGuard:
    return "perfect_guard";
  case PlayerFeedbackType::GuardBroken:
    return "guard_broken";
  case PlayerFeedbackType::DodgeSuccess:
    return "dodge_success";
  case PlayerFeedbackType::ResourceInsufficient:
    return "resource_insufficient";
  case PlayerFeedbackType::CommanderModeEntered:
    return "commander_mode_entered";
  case PlayerFeedbackType::CommanderModeExited:
    return "commander_mode_exited";
  }
  return "order_issued";
}

auto PlayerFeedbackBus::subscribe(Listener listener) -> ListenerId {
  if (!listener) {
    return 0;
  }
  const ListenerId id = m_next_listener_id++;
  m_listeners.push_back({.id = id, .listener = std::move(listener)});
  return id;
}

void PlayerFeedbackBus::unsubscribe(ListenerId id) {
  m_listeners.erase(std::remove_if(m_listeners.begin(),
                                   m_listeners.end(),
                                   [id](const Subscription& subscription) {
                                     return subscription.id == id;
                                   }),
                    m_listeners.end());
}

void PlayerFeedbackBus::publish(PlayerFeedbackEvent event) {
  event.sequence = m_next_sequence++;

  for (const auto& subscription : m_listeners) {
    if (subscription.listener) {
      subscription.listener(event);
    }
  }

  if (m_pending.size() >= k_max_pending) {
    m_pending.pop_front();
    ++m_dropped;
  }
  m_pending.push_back(std::move(event));
}

auto PlayerFeedbackBus::drain() -> std::vector<PlayerFeedbackEvent> {
  std::vector<PlayerFeedbackEvent> out(m_pending.begin(), m_pending.end());
  m_pending.clear();
  return out;
}

void PlayerFeedbackBus::clear() {
  m_pending.clear();
  m_dropped = 0;
}

auto to_variant_map(const PlayerFeedbackEvent& event) -> QVariantMap {
  QVariantMap map;
  map[QStringLiteral("type")] =
      QString::fromLatin1(player_feedback_type_name(event.type));
  map[QStringLiteral("sequence")] = static_cast<qulonglong>(event.sequence);
  map[QStringLiteral("entity")] = static_cast<qulonglong>(event.entity);
  map[QStringLiteral("strength")] = event.strength;
  map[QStringLiteral("reason")] = event.reason;
  map[QStringLiteral("hasPosition")] = event.has_world_position;
  map[QStringLiteral("x")] = event.world_position.x();
  map[QStringLiteral("y")] = event.world_position.y();
  map[QStringLiteral("z")] = event.world_position.z();
  return map;
}

} // namespace App::Core
