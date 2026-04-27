

#pragma once

#include "../animation/channel_evaluator.h"
#include "../animation/clip.h"
#include "../animation/state_machine.h"
#include "humanoid_state_machine.h"

#include <array>
#include <memory>

namespace Render::Humanoid {

struct HumanoidOverlays {

  float torso_sway_x{0.0F};

  float breathing_y{0.0F};

  float hand_jitter_amp{0.0F};
};

class HumanoidClipRegistry {
public:
  HumanoidClipRegistry();

  [[nodiscard]] auto sway_clips() const noexcept
      -> const std::vector<Render::Animation::Clip<float>> & {
    return m_sway_clips;
  }
  [[nodiscard]] auto breathing_clips() const noexcept
      -> const std::vector<Render::Animation::Clip<float>> & {
    return m_breathing_clips;
  }
  [[nodiscard]] auto jitter_clips() const noexcept
      -> const std::vector<Render::Animation::Clip<float>> & {
    return m_jitter_clips;
  }

  static auto instance() -> const HumanoidClipRegistry &;

private:
  std::vector<Render::Animation::Clip<float>> m_sway_clips;
  std::vector<Render::Animation::Clip<float>> m_breathing_clips;
  std::vector<Render::Animation::Clip<float>> m_jitter_clips;
};

class HumanoidClipDriver {
public:
  explicit HumanoidClipDriver(
      const HumanoidClipRegistry &registry = HumanoidClipRegistry::instance());

  void tick(float dt, const Render::GL::AnimationInputs &inputs,
            bool dead_flag = false);

  void snap_to(HumanoidState state) { m_state.snap_to(state); }

  [[nodiscard]] auto sample(float time) const -> HumanoidOverlays;

  [[nodiscard]] auto state() const noexcept -> HumanoidState {
    return m_state.current();
  }
  [[nodiscard]] auto
  state_machine() const noexcept -> const HumanoidStateMachine & {
    return m_state;
  }

private:
  const HumanoidClipRegistry *m_registry;
  HumanoidStateMachine m_state;
};

} // namespace Render::Humanoid
