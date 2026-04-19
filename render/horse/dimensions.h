#pragma once

#include "attachment_frames.h"
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct AnimationInputs;
struct HumanoidAnimationContext;

struct HorseDimensions {
  float body_length{};
  float body_width{};
  float body_height{};
  float barrel_center_y{};

  float neck_length{};
  float neck_rise{};

  float head_length{};
  float head_width{};
  float head_height{};
  float muzzle_length{};

  float leg_length{};
  float hoof_height{};

  float tail_length{};

  float saddle_height{};
  float saddle_thickness{};
  float seat_forward_offset{};

  float stirrup_drop{};
  float stirrup_out{};

  float idle_bob_amplitude{};
  float move_bob_amplitude{};
};

struct HorseVariant {
  QVector3D coat_color;
  QVector3D mane_color;
  QVector3D tail_color;
  QVector3D muzzle_color;
  QVector3D hoof_color;
  QVector3D saddle_color;
  QVector3D blanket_color;
  QVector3D tack_color;
};

struct HorseGait {
  float cycle_time{};
  float front_leg_phase{};
  float rear_leg_phase{};
  float stride_swing{};
  float stride_lift{};
};

struct HorseProfile {
  HorseDimensions dims{};
  HorseVariant variant;
  HorseGait gait{};
};

auto make_horse_dimensions(uint32_t seed) -> HorseDimensions;
auto make_horse_variant(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseVariant;
auto make_horse_profile(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseProfile;
auto get_or_create_cached_horse_profile(
    uint32_t seed, const QVector3D &leather_base,
    const QVector3D &cloth_base) -> HorseProfile;
void advance_horse_profile_cache_frame();
auto compute_mount_frame(const HorseProfile &profile) -> MountedAttachmentFrame;
auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx) -> ReinState;
auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D;
auto evaluate_horse_motion(HorseProfile &profile, const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx)
    -> HorseMotionSample;
void apply_mount_vertical_offset(MountedAttachmentFrame &frame, float bob);

inline void scale_horse_dimensions(HorseDimensions &dims, float scale) {
  dims.body_length *= scale;
  dims.body_width *= scale;
  dims.body_height *= scale;
  dims.neck_length *= scale;
  dims.neck_rise *= scale;
  dims.head_length *= scale;
  dims.head_width *= scale;
  dims.head_height *= scale;
  dims.muzzle_length *= scale;
  dims.leg_length *= scale;
  dims.hoof_height *= scale;
  dims.tail_length *= scale;
  dims.saddle_thickness *= scale;
  dims.seat_forward_offset *= scale;
  dims.stirrup_out *= scale;
  dims.stirrup_drop *= scale;
  dims.barrel_center_y *= scale;
  dims.saddle_height *= scale;
  dims.idle_bob_amplitude *= scale;
  dims.move_bob_amplitude *= scale;
}

} // namespace Render::GL
