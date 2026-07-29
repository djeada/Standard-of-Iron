#pragma once

#include <cstdint>

#include "clip_manifest.h"

namespace Animation {

struct HumanoidAmbientScheduleSample {
  bool active{false};
  HumanoidAmbientIdle type{HumanoidAmbientIdle::None};
  float phase{0.0F};

  float blend{0.0F};
};

inline constexpr float k_ambient_blend_in_duration = 0.40F;
inline constexpr float k_ambient_blend_out_duration = 0.45F;
inline constexpr float k_ambient_clip_duration = 3.0F;
inline constexpr float k_ambient_min_idle_duration = 5.0F;
inline constexpr float k_ambient_cooldown_min = 9.0F;
inline constexpr float k_ambient_cooldown_range = 13.0F;

inline constexpr float k_ambient_initial_spread = 18.0F;

enum class HumanoidAmbientStage : std::uint8_t {
  Dormant = 0,
  BlendIn,
  Hold,
  BlendOut,
};

inline constexpr float k_ambient_max_step = 0.25F;

struct HumanoidAmbientRuntimeState {
  HumanoidAmbientStage stage{HumanoidAmbientStage::Dormant};
  HumanoidAmbientIdle type{HumanoidAmbientIdle::None};
  float clip_phase{0.0F};
  float blend{0.0F};
  float cooldown{0.0F};
  float last_sample_time{0.0F};
  std::uint32_t play_count{0U};
  bool initialized{false};
};

struct HumanoidAmbientTickInputs {
  HumanoidAmbientRuntimeState previous{};
  float sample_time{0.0F};

  bool eligible{false};
  bool mounted{false};
  std::uint32_t seed{0U};
  float idle_duration{0.0F};
};

struct HumanoidAmbientTickSample {
  HumanoidAmbientScheduleSample sample{};
  HumanoidAmbientRuntimeState state{};
};

[[nodiscard]] auto resolve_humanoid_ambient_tick(
    const HumanoidAmbientTickInputs& inputs) noexcept -> HumanoidAmbientTickSample;

struct HumanoidAmbientSelectionInputs {
  HumanoidAmbientRuntimeState ambient_state{};
  float sample_time{0.0F};
  bool jump_active{false};
  float jump_phase{0.0F};
  bool flag_rally_active{false};
  float flag_rally_phase{0.0F};
  bool mounted{false};
  bool has_locomotion{false};
  bool attacking{false};
  bool in_hold_mode{false};
  bool guarding{false};
  bool exiting_guard{false};
  bool constructing{false};
  bool healing{false};
  bool hit_reacting{false};
  bool dying{false};
  bool dead{false};
  bool routing{false};
  std::uint32_t seed{0U};
  float idle_duration{0.0F};
};

struct HumanoidAmbientPoseInputs {
  HumanoidAmbientIdle type{HumanoidAmbientIdle::None};
  float phase{0.0F};
  bool airborne{false};
};

struct HumanoidAmbientPoseSample {
  float pelvis_x_delta{0.0F};
  float pelvis_y_delta{0.0F};
  float pelvis_z_delta{0.0F};
  float shoulder_l_x_delta{0.0F};
  float shoulder_l_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_x_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_x_delta{0.0F};
  float neck_y_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_x_delta{0.0F};
  float head_y_delta{0.0F};
  float head_z_delta{0.0F};
  float knee_l_y_delta{0.0F};
  float knee_l_z_delta{0.0F};
  float knee_r_y_delta{0.0F};
  float knee_r_z_delta{0.0F};
  float foot_l_x_delta{0.0F};
  float foot_l_y_delta{0.0F};
  float foot_l_z_delta{0.0F};
  float foot_r_x_delta{0.0F};
  float foot_r_y_delta{0.0F};
  float foot_r_z_delta{0.0F};
  float hand_l_x_delta{0.0F};
  float hand_l_y_delta{0.0F};
  float hand_l_z_delta{0.0F};
  float hand_r_x_delta{0.0F};
  float hand_r_y_delta{0.0F};
  float hand_r_z_delta{0.0F};
  float elbow_l_x_delta{0.0F};
  float elbow_l_y_delta{0.0F};
  float elbow_l_z_delta{0.0F};
  float elbow_r_x_delta{0.0F};
  float elbow_r_y_delta{0.0F};
  float elbow_r_z_delta{0.0F};
};

[[nodiscard]] auto humanoid_ambient_eligible(
    const HumanoidAmbientSelectionInputs& inputs) noexcept -> bool;

[[nodiscard]] auto resolve_humanoid_ambient_selection(
    const HumanoidAmbientSelectionInputs& inputs) noexcept -> HumanoidAmbientTickSample;

[[nodiscard]] auto resolve_humanoid_ambient_pose(
    const HumanoidAmbientPoseInputs& inputs) noexcept -> HumanoidAmbientPoseSample;

} // namespace Animation
