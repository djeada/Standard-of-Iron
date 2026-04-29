

#pragma once

#include "../animation/state_machine.h"
#include "../gl/humanoid/humanoid_types.h"

#include <cstdint>
#include <string_view>

namespace Render::Humanoid {

enum class HumanoidState : std::uint8_t {
  Idle = 0,
  Walk,
  Run,
  Hold,
  AttackMelee,
  AttackRanged,
  HitReaction,
  Healing,
  Construct,
  Death,
  Count
};

inline constexpr std::size_t k_state_count =
    static_cast<std::size_t>(HumanoidState::Count);

[[nodiscard]] auto state_name(HumanoidState s) noexcept -> std::string_view;

[[nodiscard]] auto
select_state(const Render::GL::AnimationInputs &inputs,
             bool dead_flag = false) noexcept -> HumanoidState;

class HumanoidStateMachine {
public:
  HumanoidStateMachine();

  void tick(float dt, const Render::GL::AnimationInputs &inputs,
            bool dead_flag = false);

  void snap_to(HumanoidState state);

  [[nodiscard]] auto current() const noexcept -> HumanoidState {
    return static_cast<HumanoidState>(m_machine.current());
  }
  [[nodiscard]] auto previous() const noexcept -> HumanoidState {
    return static_cast<HumanoidState>(m_machine.previous());
  }
  [[nodiscard]] auto blend_weight() const noexcept -> float {
    return m_machine.weight();
  }
  [[nodiscard]] auto is_blending() const noexcept -> bool {
    return m_machine.is_blending();
  }
  [[nodiscard]] auto
  raw() const noexcept -> const Render::Animation::StateMachine & {
    return m_machine;
  }

  [[nodiscard]] static auto
  blend_seconds_for(HumanoidState from, HumanoidState to) noexcept -> float;

private:
  Render::Animation::StateMachine m_machine;
};

} // namespace Render::Humanoid
