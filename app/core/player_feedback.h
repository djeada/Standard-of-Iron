#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

#include "game/core/entity_id.h"

namespace App::Core {

enum class PlayerFeedbackType : std::uint8_t {
  OrderIssued,
  OrderRejected,
  SelectionChanged,
  AttackCommitted,
  WeaponContact,
  PerfectGuard,
  GuardBroken,
  DodgeSuccess,
  ResourceInsufficient,
  CommanderModeEntered,
  CommanderModeExited,
};

[[nodiscard]] auto player_feedback_type_name(PlayerFeedbackType type) -> const char*;

struct PlayerFeedbackEvent {
  PlayerFeedbackType type = PlayerFeedbackType::OrderIssued;

  bool has_world_position = false;
  QVector3D world_position;

  Engine::Core::EntityID entity = 0;

  float strength = 1.0F;

  QString reason;

  std::uint64_t sequence = 0;
};

class PlayerFeedbackBus {
public:
  using Listener = std::function<void(const PlayerFeedbackEvent&)>;
  using ListenerId = std::uint64_t;

  static constexpr std::size_t k_max_pending = 64;

  auto subscribe(Listener listener) -> ListenerId;
  void unsubscribe(ListenerId id);

  void publish(PlayerFeedbackEvent event);

  [[nodiscard]] auto drain() -> std::vector<PlayerFeedbackEvent>;
  [[nodiscard]] auto pending() const -> std::size_t { return m_pending.size(); }
  [[nodiscard]] auto dropped() const -> std::uint64_t { return m_dropped; }

  void clear();

private:
  struct Subscription {
    ListenerId id = 0;
    Listener listener;
  };

  std::vector<Subscription> m_listeners;
  std::deque<PlayerFeedbackEvent> m_pending;
  ListenerId m_next_listener_id = 1;
  std::uint64_t m_next_sequence = 1;
  std::uint64_t m_dropped = 0;
};

[[nodiscard]] auto to_variant_map(const PlayerFeedbackEvent& event) -> QVariantMap;

} // namespace App::Core
