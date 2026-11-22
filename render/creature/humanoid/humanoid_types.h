#pragma once

#include <QVector3D>

#include <algorithm>
#include <cstdint>

#include "animation/action_manifest.h"
#include "animation/clip_manifest.h"
#include "animation/guard_manifest.h"
#include "animation/individuality_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/playback_manifest.h"
#include "animation/rig/side.h"
#include "game/core/component_combat.h"
#include "render/creature/animation_inputs.h"
#include "render/creature/attachment_frame.h"
#include "render/creature/combat_visual_state.h"
#include "render/creature/movement_state.h"
#include "render/frame/creature_lod.h"
#include "render/frame/palette.h"

namespace Render::GL {

struct FormationParams {
  int individuals_per_unit;
  int max_per_row;
  float spacing;
};

struct BodyFrames {
  AttachmentFrame head{};
  AttachmentFrame torso{};
  AttachmentFrame back{};
  AttachmentFrame waist{};
  AttachmentFrame shoulder_l{};
  AttachmentFrame shoulder_r{};
  AttachmentFrame hand_l{};
  AttachmentFrame hand_r{};
  AttachmentFrame shield_l{};
  AttachmentFrame grip_l{};
  AttachmentFrame grip_r{};
  AttachmentFrame foot_l{};
  AttachmentFrame foot_r{};

  AttachmentFrame shin_l{};
  AttachmentFrame shin_r{};
};

struct HumanoidPose {
  QVector3D head_pos;
  float head_r{};
  QVector3D neck_base;

  HeadFrame head_frame{};

  BodyFrames body_frames{};

  QVector3D shoulder_l, shoulder_r;
  QVector3D elbow_l, elbow_r;
  QVector3D hand_l, hand_r;

  QVector3D grip_axis_l, grip_axis_r;

  bool shield_faces_outward{false};

  QVector3D pelvis_pos;
  QVector3D knee_l, knee_r;

  float foot_y_offset{};
  QVector3D foot_l, foot_r;

  float foot_pitch_l{0.0F};
  float foot_pitch_r{0.0F};
};

struct VariationParams {
  float height_scale{1.0F};
  float bulk_scale{1.0F};
  float stance_width{1.0F};
  float arm_swing_amp{1.0F};
  float walk_speed_mult{1.0F};
  float posture_slump{0.0F};
  float shoulder_tilt{0.0F};

  static auto from_seed(uint32_t seed) -> VariationParams {
    VariationParams v{};

    auto next_rand = [](uint32_t& s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t rng = seed;
    v.height_scale = 0.95F + next_rand(rng) * 0.10F;
    v.bulk_scale = 0.92F + next_rand(rng) * 0.16F;
    v.stance_width = 0.88F + next_rand(rng) * 0.24F;
    v.arm_swing_amp = 0.85F + next_rand(rng) * 0.30F;
    v.walk_speed_mult = 0.90F + next_rand(rng) * 0.20F;
    v.posture_slump = next_rand(rng) * 0.08F;
    v.shoulder_tilt = (next_rand(rng) - 0.5F) * 0.06F;

    return v;
  }
};

struct FacialHairParams {
  FacialHairStyle style = FacialHairStyle::None;
  QVector3D color{0.15F, 0.12F, 0.10F};
  float length = 1.0F;
  float thickness = 1.0F;
  float coverage = 1.0F;
  float greyness = 0.0F;
};

struct HumanoidVariant {
  HumanoidPalette palette;
  FacialHairParams facial_hair;
  float muscularity = 1.0F;
  float scarring = 0.0F;
  float weathering = 0.0F;
  float grime = 0.0F;
  float bloodiness = 0.0F;
  float pattern_seed = 0.0F;
};

[[nodiscard]] inline auto next_seeded_unit_float(std::uint32_t& s) noexcept -> float {
  s = s * 1664525U + 1013904223U;
  return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
}

inline void seed_missing_humanoid_wear(HumanoidVariant& variant,
                                       std::uint32_t seed) noexcept {
  std::uint32_t rng = seed ^ 0xA341316CU;
  float const generated_scarring =
      next_seeded_unit_float(rng) * next_seeded_unit_float(rng) * 0.42F;
  float const generated_weathering = 0.30F + next_seeded_unit_float(rng) * 0.55F;
  float const generated_grime = 0.18F + next_seeded_unit_float(rng) * 0.52F;
  float const generated_blood = next_seeded_unit_float(rng) *
                                next_seeded_unit_float(rng) *
                                next_seeded_unit_float(rng) * 0.34F;
  float const generated_pattern = next_seeded_unit_float(rng);

  if (variant.scarring <= 0.0F) {
    variant.scarring = generated_scarring;
  }
  if (variant.weathering <= 0.0F) {
    variant.weathering = generated_weathering;
  }
  if (variant.grime <= 0.0F) {
    variant.grime = generated_grime;
  }
  if (variant.bloodiness <= 0.0F) {
    variant.bloodiness = generated_blood;
  }
  if (variant.pattern_seed <= 0.0F) {
    variant.pattern_seed = generated_pattern;
  }
}

struct HumanoidGaitDescriptor {
  HumanoidMotionState state{HumanoidMotionState::Idle};
  float speed{0.0F};
  float normalized_speed{0.0F};
  float cycle_time{0.0F};
  float cycle_phase{0.0F};
  float stride_distance{0.0F};
  QVector3D velocity{0.0F, 0.0F, 0.0F};
  bool has_target{false};
  bool is_airborne{false};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float locomotion_presence{0.0F};
  float run_presence{0.0F};
  float turn_amount{0.0F};

  float travel_alignment{1.0F};

  float travel_lateral{0.0F};
  bool reverse_gait{false};
  float acceleration{0.0F};

  auto is_stationary() const -> bool { return speed <= 0.01F; }
  auto is_walking() const -> bool { return state == HumanoidMotionState::Walk; }
  auto is_running() const -> bool { return state == HumanoidMotionState::Run; }
  auto is_holding() const -> bool { return state == HumanoidMotionState::Hold; }
  auto is_attacking() const -> bool { return state == HumanoidMotionState::Attacking; }
  bool persistent_valid{false};
  float persistent_last_sample_time{0.0F};
};

struct HumanoidAnimationContext {
  AnimationInputs inputs;
  VariationParams variation;
  FormationParams formation;

  Animation::SoldierIndividuality individuality{};
  HumanoidGaitDescriptor gait{};
  float locomotion_cycle_time{0.0F};
  float locomotion_phase{0.0F};
  float attack_phase{0.0F};
  float attack_emphasis{1.0F};
  bool amplified_attack{false};
  bool finisher_attack{false};
  float jitter_seed{0.0F};
  QVector3D entity_forward{0.0F, 0.0F, 1.0F};
  QVector3D entity_right{1.0F, 0.0F, 0.0F};
  QVector3D entity_up{0.0F, 1.0F, 0.0F};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_velocity{0.0F, 0.0F, 0.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  QVector3D instance_position{0.0F, 0.0F, 0.0F};
  float move_speed{0.0F};
  bool has_movement_target{false};
  float yaw_radians{0.0F};
  float yaw_degrees{0.0F};
  AmbientIdleType ambient_idle_type{AmbientIdleType::None};
  float ambient_idle_phase{0.0F};

  float ambient_idle_blend{0.0F};
  float idle_breath_phase{0.0F};
  ConstructionRole construction_role{ConstructionRole::None};
  float construction_blend{1.0F};

  auto locomotion_speed() const -> float { return gait.speed; }
  auto locomotion_normalized_speed() const -> float { return gait.normalized_speed; }
  auto locomotion_forward() const -> QVector3D { return locomotion_direction; }
  auto locomotion_velocity_flat() const -> QVector3D { return gait.velocity; }
  auto heading_forward() const -> QVector3D { return entity_forward; }
  auto heading_right() const -> QVector3D { return entity_right; }
  auto heading_up() const -> QVector3D { return entity_up; }
  auto is_stationary() const -> bool { return gait.is_stationary(); }
  auto is_walking() const -> bool { return gait.is_walking(); }
  auto is_running() const -> bool { return gait.is_running(); }
  auto is_holding() const -> bool { return gait.is_holding(); }
  auto is_attacking() const -> bool { return gait.is_attacking(); }
};

} // namespace Render::GL
