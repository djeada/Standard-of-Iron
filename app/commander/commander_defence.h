#pragma once

#include <cstdint>

#include "app/commander/commander_latency_probe.h"
#include "app/core/player_feedback.h"

namespace Engine::Core {
class Entity;
class TransformComponent;
class CommanderComponent;
class CommanderGuardComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

struct GuardTickInput {
  bool guard_held{false};
  bool dodging{false};
  bool airborne{false};
  float dt{0.0F};
};

class CommanderDefence {
public:
  [[nodiscard]] auto guard_was_active() const -> bool { return m_guard_was_active; }

  void advance_upkeep(Engine::Core::Entity& commander,
                      Engine::Core::CommanderComponent* commander_data,
                      bool guard_held,
                      float dt) const;

  void hold_for_rally(Engine::Core::Entity& commander, float dt);

  void apply_guard(Engine::Core::Entity& commander,
                   const GuardTickInput& input,
                   CommanderLatencyProbe* probe);

  [[nodiscard]] static auto advance_dodge_grace(Engine::Core::Entity& commander,
                                                float dt) -> float;

  void publish_resolved_feedback(Engine::Core::Entity& commander,
                                 Engine::Core::EntityID commander_id,
                                 const Engine::Core::TransformComponent& transform,
                                 PlayerFeedbackBus* bus);

  void reset() { m_guard_was_active = false; }

private:
  bool m_guard_was_active = false;
  std::uint32_t m_observed_blocked_contacts = 0;
  std::uint32_t m_observed_perfect_guard_contacts = 0;
  std::uint32_t m_observed_dodged_contacts = 0;
  std::uint32_t m_observed_guard_broken_contacts = 0;
};

} // namespace App::Core
