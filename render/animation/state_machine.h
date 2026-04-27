

#pragma once

#include <cstdint>

namespace Render::Animation {

using StateId = std::uint8_t;

class StateMachine {
public:
  StateMachine() = default;
  explicit StateMachine(StateId initial)
      : m_current(initial), m_prev(initial) {}

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
