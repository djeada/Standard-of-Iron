// Stage 12 — tiny animation state machine.
//
// Models the high-level "is this unit idle / walking / attacking"
// discrete state and the cross-fade blend between two states. The
// machine itself is unopinionated about what a state *means* — it just
// tracks which two clips to sample and with what weight.
//
// Philosophy:
//   * States are identified by a `StateId` (enum-underlying uint8_t).
//   * Transitions are handled by `request(new_state, blend_seconds)`.
//     The machine holds both the previous and current state for
//     `blend_seconds` then snaps to the new state.
//   * Clip sampling is the caller's job: the machine answers
//     "what's the current blend weight between two clips?"
//
// This keeps the state machine decoupled from the clip value type
// (Clip<float> vs Clip<QVector3D>) — a caller blends its own channels.

#pragma once

#include <cstdint>

namespace Render::Animation {

using StateId = std::uint8_t;

class StateMachine {
public:
  StateMachine() = default;
  explicit StateMachine(StateId initial) : m_current(initial), m_prev(initial) {}

  // Request a transition. If the requested state equals the current
  // state, the request is a no-op. `blend_seconds` <= 0 snaps instantly.
  void request(StateId next, float blend_seconds = 0.0F) {
    if (next == m_current && m_blend_remaining <= 0.0F) {
      return;
    }
    m_prev = m_current;
    m_current = next;
    m_blend_duration = blend_seconds > 0.0F ? blend_seconds : 0.0F;
    m_blend_remaining = m_blend_duration;
  }

  void tick(float dt) {
    if (m_blend_remaining > 0.0F) {
      m_blend_remaining -= dt;
      if (m_blend_remaining < 0.0F) {
        m_blend_remaining = 0.0F;
      }
    }
  }

  [[nodiscard]] auto current() const noexcept -> StateId { return m_current; }
  [[nodiscard]] auto previous() const noexcept -> StateId { return m_prev; }

  // 0.0 = fully in prev state, 1.0 = fully in current state.
  // Always 1.0 when no blend is in flight.
  [[nodiscard]] auto weight() const noexcept -> float {
    if (m_blend_duration <= 0.0F) {
      return 1.0F;
    }
    float const done = m_blend_duration - m_blend_remaining;
    return done / m_blend_duration;
  }

  [[nodiscard]] auto is_blending() const noexcept -> bool {
    return m_blend_remaining > 0.0F;
  }

private:
  StateId m_current{0};
  StateId m_prev{0};
  float m_blend_duration{0.0F};
  float m_blend_remaining{0.0F};
};

} // namespace Render::Animation
