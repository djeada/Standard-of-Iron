#include "elephant_renderer_base.h"
#include "prepare.h"

#include "elephant_profile_data.h"

#include "../../game/core/component.h"
#include "../creature/creature_math_utils.h"
#include "../creature/pipeline/creature_visual_definition.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "../submitter.h"
#include "elephant_spec.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>
#include <unordered_map>
#include <vector>

namespace Render::GL {

auto ElephantRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  if (!m_visual_spec_baked) {
    m_visual_spec_cache = Render::Creature::Pipeline::UnitVisualSpec{};
    m_visual_spec_cache.kind =
        Render::Creature::Pipeline::CreatureKind::Elephant;
    m_visual_spec_cache.debug_name = "elephant/default";
    m_visual_spec_cache.creature_definition =
        &Render::Creature::Pipeline::elephant_creature_visual_definition();
    const QVector3D ps = get_proportion_scaling();
    m_visual_spec_cache.scaling =
        Render::Creature::Pipeline::ProportionScaling{ps.x(), ps.y(), ps.z()};
    m_visual_spec_baked = true;
  }
  return m_visual_spec_cache;
}

namespace {

using namespace Render::Creature;

inline auto rand_between(uint32_t seed, uint32_t salt, float min_val,
                         float max_val) -> float {
  const float t = hash01(seed ^ salt);
  return min_val + (max_val - min_val) * t;
}

inline auto darken(const QVector3D &c, float k) -> QVector3D { return c * k; }

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

constexpr float k_skin_highlight_base = 0.50F;
constexpr float k_skin_vertical_factor = 0.30F;
constexpr float k_skin_longitudinal_factor = 0.15F;
constexpr float k_skin_seed_factor = 0.10F;
constexpr float k_skin_bright_factor = 1.06F;
constexpr float k_skin_shadow_factor = 0.88F;

inline auto skin_gradient(const QVector3D &skin, float vertical_factor,
                          float longitudinal_factor, float seed) -> QVector3D {
  float const highlight = saturate(
      k_skin_highlight_base + vertical_factor * k_skin_vertical_factor -
      longitudinal_factor * k_skin_longitudinal_factor +
      seed * k_skin_seed_factor);
  QVector3D const bright = lighten(skin, k_skin_bright_factor);
  QVector3D const shadow = darken(skin, k_skin_shadow_factor);
  return shadow * (1.0F - highlight) + bright * highlight;
}

} // namespace

auto make_elephant_dimensions(uint32_t seed) -> ElephantDimensions {
  using namespace ElephantDimensionRange;
  ElephantDimensions d{};

  d.body_length = rand_between(seed, k_salt_body_length, k_body_length_min,
                               k_body_length_max);
  d.body_width =
      rand_between(seed, k_salt_body_width, k_body_width_min, k_body_width_max);
  d.body_height = rand_between(seed, k_salt_body_height, k_body_height_min,
                               k_body_height_max);

  d.neck_length = rand_between(seed, k_salt_neck_length, k_neck_length_min,
                               k_neck_length_max);
  d.neck_width =
      rand_between(seed, k_salt_neck_width, k_neck_width_min, k_neck_width_max);

  d.head_length = rand_between(seed, k_salt_head_length, k_head_length_min,
                               k_head_length_max);
  d.head_width =
      rand_between(seed, k_salt_head_width, k_head_width_min, k_head_width_max);
  d.head_height = rand_between(seed, k_salt_head_height, k_head_height_min,
                               k_head_height_max);

  d.trunk_length = rand_between(seed, k_salt_trunk_length, k_trunk_length_min,
                                k_trunk_length_max);
  d.trunk_base_radius =
      rand_between(seed, k_salt_trunk_base_radius, k_trunk_base_radius_min,
                   k_trunk_base_radius_max);
  d.trunk_tip_radius =
      rand_between(seed, k_salt_trunk_tip_radius, k_trunk_tip_radius_min,
                   k_trunk_tip_radius_max);

  d.ear_width =
      rand_between(seed, k_salt_ear_width, k_ear_width_min, k_ear_width_max);
  d.ear_height =
      rand_between(seed, k_salt_ear_height, k_ear_height_min, k_ear_height_max);
  d.ear_thickness = rand_between(seed, k_salt_ear_thickness,
                                 k_ear_thickness_min, k_ear_thickness_max);

  d.leg_length =
      rand_between(seed, k_salt_leg_length, k_leg_length_min, k_leg_length_max);
  d.leg_radius =
      rand_between(seed, k_salt_leg_radius, k_leg_radius_min, k_leg_radius_max);
  d.foot_radius = rand_between(seed, k_salt_foot_radius, k_foot_radius_min,
                               k_foot_radius_max);

  d.foot_radius *= (1.0F / 1.2F);

  d.tail_length = rand_between(seed, k_salt_tail_length, k_tail_length_min,
                               k_tail_length_max);

  d.tusk_length = rand_between(seed, k_salt_tusk_length, k_tusk_length_min,
                               k_tusk_length_max);
  d.tusk_radius = rand_between(seed, k_salt_tusk_radius, k_tusk_radius_min,
                               k_tusk_radius_max);

  d.howdah_width = rand_between(seed, k_salt_howdah_width, k_howdah_width_min,
                                k_howdah_width_max);
  d.howdah_length = rand_between(seed, k_salt_howdah_length,
                                 k_howdah_length_min, k_howdah_length_max);
  d.howdah_height = rand_between(seed, k_salt_howdah_height,
                                 k_howdah_height_min, k_howdah_height_max);

  d.idle_bob_amplitude =
      rand_between(seed, k_salt_idle_bob, k_idle_bob_amplitude_min,
                   k_idle_bob_amplitude_max);
  d.move_bob_amplitude =
      rand_between(seed, k_salt_move_bob, k_move_bob_amplitude_min,
                   k_move_bob_amplitude_max);

  d.barrel_center_y =
      d.body_height * 0.30F + d.leg_length * 0.85F + d.foot_radius * 0.6F;

  return d;
}

auto make_elephant_variant(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantVariant {
  using namespace ElephantVariantConstants;
  ElephantVariant v;

  float const skin_variation = rand_between(
      seed, k_salt_skin_variation, k_skin_variation_min, k_skin_variation_max);
  v.skin_color =
      QVector3D(k_skin_base_r * skin_variation, k_skin_base_g * skin_variation,
                k_skin_base_b * skin_variation);

  float const highlight_t =
      rand_between(seed, k_salt_highlight, 0.0F, k_highlight_blend);
  v.skin_highlight = lighten(v.skin_color, 1.0F + highlight_t);

  float const shadow_t =
      rand_between(seed, k_salt_shadow, 0.0F, k_shadow_blend);
  v.skin_shadow = darken(v.skin_color, 1.0F - shadow_t);

  v.ear_inner_color = QVector3D(k_ear_inner_r, k_ear_inner_g, k_ear_inner_b);
  v.tusk_color = QVector3D(k_tusk_r, k_tusk_g, k_tusk_b);
  v.toenail_color = QVector3D(k_toenail_r, k_toenail_g, k_toenail_b);

  v.howdah_wood_color = QVector3D(k_wood_r, k_wood_g, k_wood_b);
  v.howdah_fabric_color = fabric_base;
  v.howdah_metal_color = metal_base;

  return v;
}

auto make_elephant_profile(uint32_t seed, const QVector3D &fabric_base,
                           const QVector3D &metal_base) -> ElephantProfile {
  using namespace ElephantGaitConstants;
  ElephantProfile profile;
  profile.dims = make_elephant_dimensions(seed);
  profile.variant = make_elephant_variant(seed, fabric_base, metal_base);

  profile.gait.cycle_time =
      rand_between(seed, k_salt_cycle_time, k_cycle_time_min, k_cycle_time_max);
  profile.gait.front_leg_phase =
      rand_between(seed, k_salt_front_leg_phase, k_front_leg_phase_min,
                   k_front_leg_phase_max);
  float const diagonal_lead = rand_between(
      seed, k_salt_diagonal_lead, k_diagonal_lead_min, k_diagonal_lead_max);
  profile.gait.rear_leg_phase =
      std::fmod(profile.gait.front_leg_phase + diagonal_lead, 1.0F);
  profile.gait.stride_swing = rand_between(
      seed, k_salt_stride_swing, k_stride_swing_min, k_stride_swing_max);
  profile.gait.stride_lift = rand_between(seed, k_salt_stride_lift,
                                          k_stride_lift_min, k_stride_lift_max);

  return profile;
}
} // namespace Render::GL
