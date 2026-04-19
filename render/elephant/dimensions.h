#pragma once

#include "attachment_frames.h"
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct AnimationInputs;

struct ElephantDimensions {
  float body_length{};
  float body_width{};
  float body_height{};
  float barrel_center_y{};

  float neck_length{};
  float neck_width{};

  float head_length{};
  float head_width{};
  float head_height{};

  float trunk_length{};
  float trunk_base_radius{};
  float trunk_tip_radius{};

  float ear_width{};
  float ear_height{};
  float ear_thickness{};

  float leg_length{};
  float leg_radius{};
  float foot_radius{};

  float tail_length{};

  float tusk_length{};
  float tusk_radius{};

  float howdah_width{};
  float howdah_length{};
  float howdah_height{};

  float idle_bob_amplitude{};
  float move_bob_amplitude{};
};

struct ElephantVariant {
  QVector3D skin_color;
  QVector3D skin_highlight;
  QVector3D skin_shadow;
  QVector3D ear_inner_color;
  QVector3D tusk_color;
  QVector3D toenail_color;
  QVector3D howdah_wood_color;
  QVector3D howdah_fabric_color;
  QVector3D howdah_metal_color;
};

struct ElephantGait {
  float cycle_time{};
  float front_leg_phase{};
  float rear_leg_phase{};
  float stride_swing{};
  float stride_lift{};
};

struct ElephantProfile {
  ElephantDimensions dims{};
  ElephantVariant variant;
  ElephantGait gait{};
};

auto make_elephant_dimensions(uint32_t seed) -> ElephantDimensions;
auto make_elephant_variant(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantVariant;
auto make_elephant_profile(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantProfile;
auto get_or_create_cached_elephant_profile(
    uint32_t seed, const QVector3D &fabric_base,
    const QVector3D &metal_base) -> ElephantProfile;
void advance_elephant_profile_cache_frame();
auto compute_howdah_frame(const ElephantProfile &profile)
    -> HowdahAttachmentFrame;
auto evaluate_elephant_motion(ElephantProfile &profile,
                              const AnimationInputs &anim)
    -> ElephantMotionSample;
void apply_howdah_vertical_offset(HowdahAttachmentFrame &frame, float bob);

void update_elephant_gait(ElephantGaitState &state,
                          const ElephantProfile &profile,
                          const AnimationInputs &anim,
                          const QVector3D &body_world_pos,
                          float body_forward_z);

auto solve_elephant_leg_ik(const QVector3D &hip, const QVector3D &foot_target,
                           float upper_len, float lower_len,
                           float lateral_sign) -> ElephantLegPose;

auto evaluate_swing_position(const ElephantLegState &leg,
                             float lift_height) -> QVector3D;

inline void scale_elephant_dimensions(ElephantDimensions &dims, float scale) {
  dims.body_length *= scale;
  dims.body_width *= scale;
  dims.body_height *= scale;
  dims.neck_length *= scale;
  dims.neck_width *= scale;
  dims.head_length *= scale;
  dims.head_width *= scale;
  dims.head_height *= scale;
  dims.trunk_length *= scale;
  dims.trunk_base_radius *= scale;
  dims.trunk_tip_radius *= scale;
  dims.ear_width *= scale;
  dims.ear_height *= scale;
  dims.ear_thickness *= scale;
  dims.leg_length *= scale;
  dims.leg_radius *= scale;
  dims.foot_radius *= scale;
  dims.tail_length *= scale;
  dims.tusk_length *= scale;
  dims.tusk_radius *= scale;
  dims.howdah_width *= scale;
  dims.howdah_length *= scale;
  dims.howdah_height *= scale;
  dims.barrel_center_y *= scale;
  dims.idle_bob_amplitude *= scale;
  dims.move_bob_amplitude *= scale;
}

} // namespace Render::GL
