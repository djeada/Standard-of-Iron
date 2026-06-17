#pragma once

namespace Animation {

enum class HorseGaitType {
  Idle,
  Walk,
  Trot,
  Canter,
  Gallop,
};

struct HorseGaitSelectionInputs {
  bool has_locomotion_input{false};
  bool anim_has_motion{false};
  bool anim_has_run{false};
  float speed{0.0F};
  HorseGaitType anchor{HorseGaitType::Idle};
};

struct HorseGaitTransitionInputs {
  HorseGaitType current{HorseGaitType::Idle};
  HorseGaitType target{HorseGaitType::Idle};
  float transition_progress{1.0F};
  float transition_start_time{0.0F};
  float sample_time{0.0F};
  HorseGaitType desired{HorseGaitType::Idle};
  float idle_bob_intensity{1.0F};
  float transition_duration{0.30F};
};

struct HorseGaitTransitionSample {
  HorseGaitType current{HorseGaitType::Idle};
  HorseGaitType target{HorseGaitType::Idle};
  float transition_progress{1.0F};
  float transition_start_time{0.0F};
  float idle_bob_intensity{1.0F};
};

[[nodiscard]] auto resolve_horse_desired_gait(
    const HorseGaitSelectionInputs& inputs) noexcept -> HorseGaitType;

[[nodiscard]] auto resolve_horse_gait_transition(
    const HorseGaitTransitionInputs& inputs) noexcept -> HorseGaitTransitionSample;

[[nodiscard]] auto
horse_playback_gait_for_transition(HorseGaitType current,
                                   HorseGaitType target,
                                   bool is_moving) noexcept -> HorseGaitType;

[[nodiscard]] auto horse_bob_scale_for_gait(HorseGaitType gait) noexcept -> float;

} // namespace Animation
