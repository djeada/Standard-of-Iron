#pragma once

#include <QVector3D>

#include <cstdint>

#include "animation/rig/horse_attachment_frames.h"
#include "animation/rig/horse_gait.h"

namespace Render::GL {

// One knob for how large a horse is, applied to both the authored mesh and the
// dimensions the saddle, stirrups and rider seat are derived from. The two must
// move together: the seat is positioned from dimensions but follows a mesh
// bone, so scaling one without the other leaves the rider hovering.
//
// The authored horse stood 2.13 units at the withers against a 1.80 unit man -
// taller at the shoulder than a person is tall overall. This brings the withers
// to about 1.57, roughly where a warhorse sits against its rider.
inline constexpr float k_horse_scale = 0.74F;

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

enum class HorseCoatKind : std::uint8_t {
  Bay = 0,
  Chestnut,
  Black,
  DappleGrey,
  Palomino,
  Dun,
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

  HorseCoatKind coat_kind{HorseCoatKind::Bay};
  float dapple_amount{0.0F};
  std::uint8_t sock_mask{0U};
  bool has_blaze{false};
  bool has_star{false};
};

struct HorseProfile {
  HorseDimensions dims{};
  HorseVariant variant;
  HorseGait gait{};
};

auto make_horse_dimensions(uint32_t seed) -> HorseDimensions;
auto make_horse_variant(uint32_t seed,
                        const QVector3D& leather_base,
                        const QVector3D& cloth_base) -> HorseVariant;
auto make_horse_profile(uint32_t seed,
                        const QVector3D& leather_base,
                        const QVector3D& cloth_base) -> HorseProfile;

inline void scale_horse_dimensions(HorseDimensions& dims, float scale) {
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
  // stirrup_drop is deliberately not scaled: it is how far the rider's leg
  // reaches, not how big the horse is. A smaller mount does not give its rider
  // shorter legs, and scaling it here is what left riders crouched like
  // jockeys with their feet tucked up under them.
  dims.barrel_center_y *= scale;
  dims.saddle_height *= scale;
  dims.idle_bob_amplitude *= scale;
  dims.move_bob_amplitude *= scale;
}

} // namespace Render::GL
