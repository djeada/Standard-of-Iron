#include "humanoid_state_machine.h"

#include <array>

namespace Render::Humanoid {

namespace {

constexpr std::array<std::string_view, k_state_count> k_state_names = {
    "Idle",         "Walk",        "Run",     "Hold",      "AttackMelee",
    "AttackRanged", "HitReaction", "Healing", "Construct", "Death",
};

}

auto state_name(HumanoidState s) noexcept -> std::string_view {
  auto const i = static_cast<std::size_t>(s);
  return i < k_state_count ? k_state_names[i] : std::string_view{"<invalid>"};
}

auto select_state(const Render::GL::AnimationInputs &inputs,
                  bool dead_flag) noexcept -> HumanoidState {
  if (dead_flag) {
    return HumanoidState::Death;
  }
  if (inputs.is_hit_reacting) {
    return HumanoidState::HitReaction;
  }
  if (inputs.is_attacking) {
    return inputs.is_melee ? HumanoidState::AttackMelee
                           : HumanoidState::AttackRanged;
  }
  if (inputs.is_constructing) {
    return HumanoidState::Construct;
  }
  if (inputs.is_healing) {
    return HumanoidState::Healing;
  }
  if (inputs.is_in_hold_mode) {
    return HumanoidState::Hold;
  }
  if (inputs.is_running) {
    return HumanoidState::Run;
  }
  if (inputs.is_moving) {
    return HumanoidState::Walk;
  }
  return HumanoidState::Idle;
}

HumanoidStateMachine::HumanoidStateMachine()
    : m_machine(static_cast<Render::Animation::StateId>(HumanoidState::Idle)) {}

auto HumanoidStateMachine::blend_seconds_for(
    HumanoidState from, HumanoidState to) noexcept -> float {
  if (from == to) {
    return 0.0F;
  }

  if (to == HumanoidState::Death || from == HumanoidState::Death) {
    return 0.0F;
  }

  if (to == HumanoidState::HitReaction) {
    return 0.05F;
  }
  if (from == HumanoidState::HitReaction) {
    return 0.15F;
  }

  if (to == HumanoidState::AttackMelee || to == HumanoidState::AttackRanged) {
    return 0.12F;
  }

  return 0.20F;
}

void HumanoidStateMachine::tick(float dt,
                                const Render::GL::AnimationInputs &inputs,
                                bool dead_flag) {
  m_machine.tick(dt);
  HumanoidState const desired = select_state(inputs, dead_flag);
  HumanoidState const cur = current();
  if (desired != cur) {
    m_machine.request(static_cast<Render::Animation::StateId>(desired),
                      blend_seconds_for(cur, desired));
  }
}

void HumanoidStateMachine::snap_to(HumanoidState state) {
  m_machine = Render::Animation::StateMachine(
      static_cast<Render::Animation::StateId>(state));
}

} // namespace Render::Humanoid
