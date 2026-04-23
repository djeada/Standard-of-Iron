#include "horse_renderer_base.h"
#include "lod.h"
#include "prepare.h"
#include "render_stats.h"

#include "horse_profile_data.h"

#include "../creature/creature_math_utils.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "../submitter.h"
#include "horse_spec.h"

#include <vector>

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL {

auto HorseRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  if (!m_visual_spec_baked) {
    m_visual_spec_cache = Render::Creature::Pipeline::UnitVisualSpec{};
    m_visual_spec_cache.kind = Render::Creature::Pipeline::CreatureKind::Horse;
    m_visual_spec_cache.debug_name = "horse/default";
    const QVector3D ps = get_proportion_scaling();
    m_visual_spec_cache.scaling =
        Render::Creature::Pipeline::ProportionScaling{ps.x(), ps.y(), ps.z()};
    m_visual_spec_baked = true;
  }
  return m_visual_spec_cache;
}

using Render::Geom::lerp;

namespace {

using namespace Render::Creature;

inline auto rand_between(uint32_t seed, uint32_t salt, float min_val,
                         float max_val) -> float {
  const float t = hash01(seed ^ salt);
  return min_val + (max_val - min_val) * t;
}

inline auto darken(const QVector3D &c, float k) -> QVector3D { return c * k; }

constexpr float k_coat_highlight_base = 0.55F;
constexpr float k_coat_vertical_factor = 0.35F;
constexpr float k_coat_longitudinal_factor = 0.20F;
constexpr float k_coat_seed_factor = 0.08F;
constexpr float k_coat_bright_factor = 1.08F;
constexpr float k_coat_shadow_factor = 0.86F;

inline auto coat_gradient(const QVector3D &coat, float vertical_factor,
                          float longitudinal_factor, float seed) -> QVector3D {
  float const highlight = saturate(
      k_coat_highlight_base + vertical_factor * k_coat_vertical_factor -
      longitudinal_factor * k_coat_longitudinal_factor +
      seed * k_coat_seed_factor);
  QVector3D const bright = lighten(coat, k_coat_bright_factor);
  QVector3D const shadow = darken(coat, k_coat_shadow_factor);
  return shadow * (1.0F - highlight) + bright * highlight;
}

inline auto lerp3(const QVector3D &a, const QVector3D &b,
                  float t) -> QVector3D {
  return {a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t,
          a.z() + (b.z() - a.z()) * t};
}

inline auto bezier(const QVector3D &p0, const QVector3D &p1,
                   const QVector3D &p2, float t) -> QVector3D {
  float const u = 1.0F - t;
  return p0 * (u * u) + p1 * (2.0F * u * t) + p2 * (t * t);
}

} // namespace

auto make_horse_dimensions(uint32_t seed) -> HorseDimensions {
  using namespace HorseDimensionRange;
  HorseDimensions d{};

  d.body_length =
      rand_between(seed, kSaltBodyLength, kBodyLengthMin, kBodyLengthMax);
  d.body_width =
      rand_between(seed, kSaltBodyWidth, kBodyWidthMin, kBodyWidthMax);
  d.body_height =
      rand_between(seed, kSaltBodyHeight, kBodyHeightMin, kBodyHeightMax);

  d.neck_length =
      rand_between(seed, kSaltNeckLength, kNeckLengthMin, kNeckLengthMax);
  d.neck_rise = rand_between(seed, kSaltNeckRise, kNeckRiseMin, kNeckRiseMax);
  d.head_length =
      rand_between(seed, kSaltHeadLength, kHeadLengthMin, kHeadLengthMax);
  d.head_width =
      rand_between(seed, kSaltHeadWidth, kHeadWidthMin, kHeadWidthMax);
  d.head_height =
      rand_between(seed, kSaltHeadHeight, kHeadHeightMin, kHeadHeightMax);
  d.muzzle_length =
      rand_between(seed, kSaltMuzzleLength, kMuzzleLengthMin, kMuzzleLengthMax);

  d.leg_length =
      rand_between(seed, kSaltLegLength, kLegLengthMin, kLegLengthMax);
  d.hoof_height =
      rand_between(seed, kSaltHoofHeight, kHoofHeightMin, kHoofHeightMax);

  d.tail_length =
      rand_between(seed, kSaltTailLength, kTailLengthMin, kTailLengthMax);

  d.saddle_thickness = rand_between(seed, kSaltSaddleThickness,
                                    kSaddleThicknessMin, kSaddleThicknessMax);
  d.seat_forward_offset =
      rand_between(seed, kSaltSeatForwardOffset, kSeatForwardOffsetMin,
                   kSeatForwardOffsetMax);
  d.stirrup_out =
      d.body_width * rand_between(seed, kSaltStirrupOut, kStirrupOutScaleMin,
                                  kStirrupOutScaleMax);
  d.stirrup_drop =
      rand_between(seed, kSaltStirrupDrop, kStirrupDropMin, kStirrupDropMax);

  d.idle_bob_amplitude = rand_between(seed, kSaltIdleBob, kIdleBobAmplitudeMin,
                                      kIdleBobAmplitudeMax);
  d.move_bob_amplitude = rand_between(seed, kSaltMoveBob, kMoveBobAmplitudeMin,
                                      kMoveBobAmplitudeMax);

  float const avg_leg_segment_ratio =
      kLegSegmentRatioUpper + kLegSegmentRatioMiddle + kLegSegmentRatioLower;
  float const leg_down_distance =
      d.leg_length * avg_leg_segment_ratio + d.hoof_height;
  float const shoulder_to_barrel_offset =
      d.body_height * kShoulderBarrelOffsetScale + kShoulderBarrelOffsetBase;
  d.barrel_center_y = leg_down_distance - shoulder_to_barrel_offset;

  d.saddle_height = d.barrel_center_y + d.body_height * kSaddleHeightBodyScale +
                    d.saddle_thickness;

  return d;
}

auto make_horse_variant(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseVariant {
  using namespace HorseVariantConstants;
  HorseVariant v;

  float const coat_hue = hash01(seed ^ kSaltCoatHue);
  if (coat_hue < 0.34F) {
    v.coat_kind = HorseCoatKind::Bay;
    v.coat_color = QVector3D(0.46F, 0.29F, 0.17F);
  } else if (coat_hue < 0.64F) {
    v.coat_kind = HorseCoatKind::Chestnut;
    v.coat_color = QVector3D(0.58F, 0.33F, 0.19F);
  } else if (coat_hue < 0.76F) {
    v.coat_kind = HorseCoatKind::DappleGrey;
    v.coat_color = QVector3D(0.72F, 0.71F, 0.68F);
    v.dapple_amount = rand_between(seed, 0xD421EU, 0.20F, 0.55F);
  } else if (coat_hue < 0.80F) {
    v.coat_kind = HorseCoatKind::Black;
    v.coat_color = QVector3D(0.20F, 0.17F, 0.14F);
  } else if (coat_hue < 0.92F) {
    v.coat_kind = HorseCoatKind::Dun;
    v.coat_color = QVector3D(0.64F, 0.54F, 0.35F);
  } else {
    v.coat_kind = HorseCoatKind::Palomino;
    v.coat_color = QVector3D(0.84F, 0.68F, 0.40F);
  }

  float const hue_jitter = (hash01(seed ^ 0x55AAU) - 0.5F) * 0.06F;
  v.coat_color = QVector3D(saturate(v.coat_color.x() + hue_jitter * 0.8F),
                           saturate(v.coat_color.y() + hue_jitter * 0.6F),
                           saturate(v.coat_color.z() + hue_jitter * 0.4F));

  switch (v.coat_kind) {
  case HorseCoatKind::Bay:
  case HorseCoatKind::Black:
    v.mane_color = QVector3D(0.06F, 0.05F, 0.05F);
    v.tail_color = QVector3D(0.06F, 0.05F, 0.05F);
    break;
  case HorseCoatKind::Chestnut:

    if (hash01(seed ^ 0x9090U) > 0.78F) {
      v.mane_color = QVector3D(0.86F, 0.74F, 0.52F);
    } else {
      v.mane_color = v.coat_color * 0.85F;
    }
    v.tail_color = v.mane_color;
    break;
  case HorseCoatKind::DappleGrey:
    v.mane_color = QVector3D(0.30F, 0.28F, 0.27F);
    v.tail_color = QVector3D(0.20F, 0.19F, 0.18F);
    break;
  case HorseCoatKind::Palomino:
    v.mane_color = QVector3D(0.96F, 0.92F, 0.82F);
    v.tail_color = QVector3D(0.94F, 0.90F, 0.80F);
    break;
  case HorseCoatKind::Dun:
    v.mane_color = QVector3D(0.18F, 0.15F, 0.10F);
    v.tail_color = QVector3D(0.18F, 0.15F, 0.10F);
    break;
  }

  float const marking_roll = hash01(seed ^ kSaltBlazeChance);
  v.has_blaze = marking_roll > 0.78F;
  v.has_star = !v.has_blaze && marking_roll > 0.55F;

  v.sock_mask = 0U;
  for (std::uint8_t leg = 0U; leg < 4U; ++leg) {
    if (hash01(seed ^ (0x1A2BU + leg * 17U)) > 0.82F) {
      v.sock_mask |= static_cast<std::uint8_t>(1U << leg);
    }
  }

  v.muzzle_color =
      lerp(v.coat_color, QVector3D(kMuzzleBaseR, kMuzzleBaseG, kMuzzleBaseB),
           kMuzzleBlendFactor);
  if (v.has_blaze) {
    v.muzzle_color =
        lerp(v.muzzle_color, QVector3D(0.92F, 0.90F, 0.86F), 0.35F);
  } else if (v.has_star) {
    v.muzzle_color =
        lerp(v.muzzle_color, QVector3D(0.88F, 0.86F, 0.82F), 0.18F);
  }

  QVector3D const dark_hoof(0.14F, 0.12F, 0.10F);
  QVector3D const pale_hoof(0.78F, 0.74F, 0.66F);
  bool const any_sock = v.sock_mask != 0U;
  v.hoof_color = any_sock
                     ? lerp(dark_hoof, pale_hoof,
                            rand_between(seed, kSaltHoofBlend, 0.30F, 0.55F))
                     : lerp(dark_hoof, QVector3D(0.32F, 0.28F, 0.24F),
                            rand_between(seed, kSaltHoofBlend, 0.10F, 0.45F));

  float const leather_tone =
      rand_between(seed, kSaltLeatherTone, kLeatherToneMin, kLeatherToneMax);
  float const tack_tone =
      rand_between(seed, kSaltTackTone, kTackToneMin, kTackToneMax);
  QVector3D const leather_tint = leather_base * leather_tone;
  QVector3D tack_tint = leather_base * tack_tone;
  if (marking_roll > kSpecialTackThreshold) {
    tack_tint =
        lerp(tack_tint, QVector3D(kSpecialTackR, kSpecialTackG, kSpecialTackB),
             kSpecialTackBlend);
  }
  v.saddle_color = leather_tint;
  v.tack_color = tack_tint;

  v.blanket_color = cloth_base * rand_between(seed, kSaltBlanketTint,
                                              kBlanketTintMin, kBlanketTintMax);

  return v;
}

auto make_horse_profile(uint32_t seed, const QVector3D &leather_base,
                        const QVector3D &cloth_base) -> HorseProfile {
  using namespace HorseGaitConstants;
  HorseProfile profile;
  profile.dims = make_horse_dimensions(seed);
  profile.variant = make_horse_variant(seed, leather_base, cloth_base);

  profile.gait.cycle_time =
      rand_between(seed, kSaltCycleTime, kCycleTimeMin, kCycleTimeMax);
  profile.gait.front_leg_phase = rand_between(
      seed, kSaltFrontLegPhase, kFrontLegPhaseMin, kFrontLegPhaseMax);
  float const diagonal_lead =
      rand_between(seed, kSaltDiagonalLead, kDiagonalLeadMin, kDiagonalLeadMax);
  profile.gait.rear_leg_phase =
      std::fmod(profile.gait.front_leg_phase + diagonal_lead, 1.0F);
  profile.gait.stride_swing =
      rand_between(seed, kSaltStrideSwing, kStrideSwingMin, kStrideSwingMax);
  profile.gait.stride_lift =
      rand_between(seed, kSaltStrideLift, kStrideLiftMin, kStrideLiftMax);

  profile.gait.phase_offset = hash01(seed ^ 0xF105E7U);
  profile.gait.stride_jitter = (hash01(seed ^ 0xA17B2U) - 0.5F) * 0.10F;
  profile.gait.head_height_jitter = (hash01(seed ^ 0xC0DECU) - 0.5F) * 0.06F;
  profile.gait.lateral_lead_front = 0.5F;
  profile.gait.lateral_lead_rear = 0.5F;
  profile.gait.ear_pin = 0.0F;

  return profile;
}

auto MountedAttachmentFrame::stirrup_attach(bool is_left) const
    -> const QVector3D & {
  return is_left ? stirrup_attach_left : stirrup_attach_right;
}

auto MountedAttachmentFrame::stirrup_bottom(bool is_left) const
    -> const QVector3D & {
  return is_left ? stirrup_bottom_left : stirrup_bottom_right;
}

} // namespace Render::GL
