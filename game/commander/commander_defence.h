#pragma once

#include <cstdint>

#include "game/commander/commander_latency_probe.h"
#include "game/core/player_feedback.h"

namespace Engine::Core {
class Entity;
class TransformComponent;
class CommanderComponent;
class CommanderGuardComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Commander {

struct GuardTickInput {
  bool guard_held{false};
  bool dodging{false};
  bool airborne{false};
  bool staggered{false};
  float dt{0.0F};
};

class CommanderDefence {
public:
  [[nodiscard]] auto guard_was_active() const -> bool { return m_guard_was_active; }

  static void
  advance_upkeep(Engine::Core::Entity& commander, bool guard_held, float dt);

  void hold_for_rally(Engine::Core::Entity& commander, float dt);

  void apply_guard(Engine::Core::Entity& commander,
                   const GuardTickInput& input,
                   CommanderLatencyProbe* probe);

  [[nodiscard]] static auto advance_dodge_grace(Engine::Core::Entity& commander,
                                                float dt) -> float;

  void publish_resolved_feedback(Engine::Core::Entity& commander,
                                 Engine::Core::EntityID commander_id,
                                 const Engine::Core::TransformComponent& transform,
                                 Game::Core::PlayerFeedbackBus* bus);

  void reset() {
    m_guard_was_active = false;
    m_contacts_adopted = false;
  }

private:
  friend class CommanderRuntime;

  bool m_guard_was_active = false;

  bool m_contacts_adopted = false;
  std::uint32_t m_observed_blocked_contacts = 0;
  std::uint32_t m_observed_perfect_guard_contacts = 0;
  std::uint32_t m_observed_dodged_contacts = 0;
  std::uint32_t m_observed_guard_broken_contacts = 0;
};

} // namespace Game::Commander
