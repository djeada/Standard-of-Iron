#pragma once

#include <algorithm>

namespace Render::GL::BackendPipelines {

class SkyBoxTransition {
public:
  static constexpr float k_transition_seconds = 0.45F;
  static constexpr float k_max_step_seconds = 0.25F;

  void set_target(bool sky_box_visible) noexcept {
    m_target = sky_box_visible ? 1.0F : 0.0F;
  }

  auto advance(float delta_seconds) noexcept -> float {
    const float step =
        std::clamp(delta_seconds, 0.0F, k_max_step_seconds) / k_transition_seconds;
    if (m_progress < m_target) {
      m_progress = std::min(m_target, m_progress + step);
    } else if (m_progress > m_target) {
      m_progress = std::max(m_target, m_progress - step);
    }
    return blend();
  }

  [[nodiscard]] auto progress() const noexcept -> float { return m_progress; }

  [[nodiscard]] auto blend() const noexcept -> float {
    return m_progress * m_progress * (3.0F - 2.0F * m_progress);
  }

  [[nodiscard]] auto is_visible() const noexcept -> bool { return m_progress > 0.0F; }

  [[nodiscard]] auto is_settled() const noexcept -> bool {
    return m_progress == m_target;
  }

private:
  float m_target{0.0F};
  float m_progress{0.0F};
};

} // namespace Render::GL::BackendPipelines
