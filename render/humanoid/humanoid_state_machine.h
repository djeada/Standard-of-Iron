// Stage 15 — humanoid state machine.
//
// Sits between gameplay-level AnimationInputs (is_moving, is_attacking,
// combat_phase, hit_reaction, ...) and the generic Render::Animation
// StateMachine (which is unopinionated about what states mean). This
// class is the one place that encodes "what does this unit's flag soup
// become as a discrete animation state id", so the rest of the renderer
// can consume a single enum.
//
// Design notes:
//   * States are discrete. Blending between them is handled by the
//     generic StateMachine's weight() accessor and by per-channel
//     ChannelEvaluators in the humanoid clip registry.
//   * Transitions are priority-ordered (hit > attack > construct >
//     heal > locomotion) so a new flag never silently outranks an
//     existing one.
//   * The mapping is pure: identical AnimationInputs always produce
//     the same requested state. All stateful behaviour (blend timers,
//     previous-state memory) lives in the owned Render::Animation::
//     StateMachine instance.

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
  Hold,          // Formation hold / parade rest
  AttackMelee,
  AttackRanged,
  HitReaction,
  Healing,
  Construct,
  Death,
  Count
};

inline constexpr std::size_t kStateCount =
    static_cast<std::size_t>(HumanoidState::Count);

[[nodiscard]] auto state_name(HumanoidState s) noexcept -> std::string_view;

// Pick the discrete state that best represents the given AnimationInputs.
// Priority: hit_reaction > attack > construct > heal > hold > run > walk >
// idle. Death is orthogonal — callers must pass it explicitly via
// `dead_flag` because HumanoidPose/AnimationInputs does not carry a
// "this unit is dead" bit directly.
[[nodiscard]] auto select_state(const Render::GL::AnimationInputs &inputs,
                                bool dead_flag = false) noexcept
    -> HumanoidState;

// Thin wrapper around Render::Animation::StateMachine that auto-requests
// transitions from AnimationInputs and exposes the current/previous
// HumanoidState in typed form.
class HumanoidStateMachine {
public:
  HumanoidStateMachine();

  // Advance blend timer and, if inputs imply a different state, request
  // a transition with the stage-appropriate default blend time.
  void tick(float dt, const Render::GL::AnimationInputs &inputs,
            bool dead_flag = false);

  // Force an immediate snap to a state — useful for spawn / despawn /
  // test setup. Skips cross-fading.
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
  [[nodiscard]] auto raw() const noexcept
      -> const Render::Animation::StateMachine & {
    return m_machine;
  }

  // Blend duration (seconds) used for transitions into a given state.
  // Exposed so callers + tests can verify the policy.
  [[nodiscard]] static auto blend_seconds_for(HumanoidState from,
                                              HumanoidState to) noexcept
      -> float;

private:
  Render::Animation::StateMachine m_machine;
};

} // namespace Render::Humanoid
