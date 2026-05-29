#include "sword_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>

#include "../../entity/renderer_constants.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/skeleton.h"
#include "../../humanoid/style_palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "../oriented_archetype_utils.h"
#include "../primitive_archetype_utils.h"
#include "math/math_utils.h"

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::Geom::cylinder_between;
using Render::Geom::ease_in_out_cubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;
using Render::Geom::sphere_at;

namespace {

enum SwordPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_metal_dark_slot = 1U,
  k_fuller_slot = 2U,
  k_leather_slot = 3U,
};

struct SwordArchetypeKey {
  int sword_length_key{0};
  int sword_width_key{0};
  int guard_half_width_key{0};
  int handle_radius_key{0};
  int pommel_radius_key{0};
  int pommel_length_key{0};
  int blade_ricasso_key{0};
  int blade_taper_bias_key{0};
  int blade_mid_width_scale_key{0};
  int blade_tip_width_scale_key{0};
  int blade_curve_key{0};
  int guard_curve_key{0};
  int guard_spike_length_key{0};
  int material_id{0};
};

auto operator==(const SwordArchetypeKey& lhs, const SwordArchetypeKey& rhs) -> bool {
  return lhs.sword_length_key == rhs.sword_length_key &&
         lhs.sword_width_key == rhs.sword_width_key &&
         lhs.guard_half_width_key == rhs.guard_half_width_key &&
         lhs.handle_radius_key == rhs.handle_radius_key &&
         lhs.pommel_radius_key == rhs.pommel_radius_key &&
         lhs.pommel_length_key == rhs.pommel_length_key &&
         lhs.blade_ricasso_key == rhs.blade_ricasso_key &&
         lhs.blade_taper_bias_key == rhs.blade_taper_bias_key &&
         lhs.blade_mid_width_scale_key == rhs.blade_mid_width_scale_key &&
         lhs.blade_tip_width_scale_key == rhs.blade_tip_width_scale_key &&
         lhs.blade_curve_key == rhs.blade_curve_key &&
         lhs.guard_curve_key == rhs.guard_curve_key &&
         lhs.guard_spike_length_key == rhs.guard_spike_length_key &&
         lhs.material_id == rhs.material_id;
}

auto quantize_sword_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

auto sword_basis_transform(const QMatrix4x4& parent,
                           const QVector3D& origin,
                           const QVector3D& blade_axis,
                           const QVector3D& guard_axis) -> QMatrix4x4 {
  QVector3D y_axis = blade_axis;
  if (y_axis.lengthSquared() <= 1e-6F) {
    return parent;
  }
  y_axis.normalize();

  QVector3D x_axis = guard_axis;
  if (x_axis.lengthSquared() <= 1e-6F) {
    x_axis = QVector3D::crossProduct(QVector3D(0.0F, 1.0F, 0.0F), y_axis);
  }
  if (x_axis.lengthSquared() <= 1e-6F) {
    x_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  x_axis.normalize();

  QVector3D z_axis = QVector3D::crossProduct(x_axis, y_axis);
  if (z_axis.lengthSquared() <= 1e-6F) {
    z_axis = QVector3D::crossProduct(x_axis, QVector3D(0.0F, 0.0F, 1.0F));
  }
  z_axis.normalize();

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(x_axis, 0.0F));
  local.setColumn(1, QVector4D(y_axis, 0.0F));
  local.setColumn(2, QVector4D(z_axis, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

auto oriented_box_between(const QVector3D& start,
                          const QVector3D& end,
                          float half_width,
                          float half_depth,
                          QVector3D right_hint = QVector3D(1.0F,
                                                           0.0F,
                                                           0.0F)) -> QMatrix4x4 {
  QVector3D y_axis = end - start;
  float const length = y_axis.length();
  if (length <= 1e-5F) {
    return box_local_model((start + end) * 0.5F,
                           QVector3D(half_width, half_width, half_depth));
  }
  y_axis /= length;

  QVector3D x_axis = right_hint - y_axis * QVector3D::dotProduct(right_hint, y_axis);
  if (x_axis.lengthSquared() <= 1e-6F) {
    x_axis = QVector3D::crossProduct(QVector3D(0.0F, 0.0F, 1.0F), y_axis);
  }
  if (x_axis.lengthSquared() <= 1e-6F) {
    x_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  x_axis.normalize();

  QVector3D z_axis = QVector3D::crossProduct(x_axis, y_axis);
  if (z_axis.lengthSquared() <= 1e-6F) {
    z_axis = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    z_axis.normalize();
  }

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(x_axis * half_width, 0.0F));
  local.setColumn(1, QVector4D(y_axis * (length * 0.5F), 0.0F));
  local.setColumn(2, QVector4D(z_axis * half_depth, 0.0F));
  local.setColumn(3, QVector4D((start + end) * 0.5F, 1.0F));
  return local;
}

auto hand_basis_transform(const QMatrix4x4& parent,
                          const AttachmentFrame& hand) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(hand.right, 0.0F));
  local.setColumn(1, QVector4D(hand.up, 0.0F));
  local.setColumn(2, QVector4D(hand.forward, 0.0F));
  local.setColumn(3, QVector4D(hand.origin, 1.0F));
  return parent * local;
}

auto sword_local_pose(const QVector3D& blade_axis_local) -> QMatrix4x4 {
  QVector3D blade_dir = blade_axis_local;
  if (blade_dir.lengthSquared() <= 1e-6F) {
    blade_dir = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    blade_dir.normalize();
  }

  QVector3D guard_right(0.0F, 0.0F, 1.0F);
  guard_right -= blade_dir * QVector3D::dotProduct(guard_right, blade_dir);
  if (guard_right.lengthSquared() <= 1e-6F) {
    guard_right = QVector3D(0.0F, 0.0F, 1.0F);
    guard_right -= blade_dir * QVector3D::dotProduct(guard_right, blade_dir);
  }
  guard_right.normalize();

  QVector3D z_axis = QVector3D::crossProduct(guard_right, blade_dir);
  if (z_axis.lengthSquared() <= 1e-6F) {
    z_axis = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    z_axis.normalize();
  }

  QMatrix4x4 pose;
  pose.setColumn(0, QVector4D(guard_right, 0.0F));
  pose.setColumn(1, QVector4D(blade_dir, 0.0F));
  pose.setColumn(2, QVector4D(z_axis, 0.0F));
  pose.setColumn(3, QVector4D(blade_dir * 0.05F, 1.0F));
  return pose;
}

auto sword_archetype(const SwordRenderConfig& config) -> const RenderArchetype& {
  struct CachedArchetype {
    SwordArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;

  SwordArchetypeKey const key{quantize_sword_value(config.sword_length),
                              quantize_sword_value(config.sword_width),
                              quantize_sword_value(config.guard_half_width),
                              quantize_sword_value(config.handle_radius),
                              quantize_sword_value(config.pommel_radius),
                              quantize_sword_value(config.pommel_length),
                              quantize_sword_value(config.blade_ricasso),
                              quantize_sword_value(config.blade_taper_bias),
                              quantize_sword_value(config.blade_mid_width_scale),
                              quantize_sword_value(config.blade_tip_width_scale),
                              quantize_sword_value(config.blade_curve),
                              quantize_sword_value(config.guard_curve),
                              quantize_sword_value(config.guard_spike_length),
                              config.material_id};
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const l = config.sword_length;
  float const base_w = config.sword_width;
  float const base_half_w = base_w * 0.5F;
  float const blade_thickness = base_w * 0.16F;
  float const ricasso_len = clamp_f(config.blade_ricasso, 0.10F, l * 0.30F);
  float const taper_bias = clamp01(config.blade_taper_bias);
  float const mid_scale = clamp_f(config.blade_mid_width_scale, 0.70F, 1.45F);
  float const tip_scale = clamp_f(config.blade_tip_width_scale, 0.05F, 0.50F);
  float const curve = config.blade_curve;
  float const mid_half_w = base_half_w * mid_scale;
  float const upper_half_w = lerp(mid_half_w,
                                  base_half_w * 0.64F,
                                  clamp_f((taper_bias - 0.25F) * 0.9F, 0.0F, 1.0F));
  float const tip_half_w = std::max(base_half_w * tip_scale, blade_thickness * 0.65F);
  float const belly_y = lerp(ricasso_len, l, 0.34F + taper_bias * 0.18F);
  float const tip_start_dist = lerp(belly_y, l, 0.44F + (1.0F - tip_scale) * 0.18F);
  float const handle_len = clamp_f(base_w * 1.35F, 0.09F, 0.16F);
  float const pommel_y = -(handle_len + config.pommel_radius * 0.25F);
  float const guard_depth = base_w * 0.18F;
  float const guard_end_r = base_w * 0.20F;
  QVector3D const blade0(0.0F, 0.0F, 0.0F);
  QVector3D const blade1(curve * 0.08F, ricasso_len, 0.0F);
  QVector3D const blade2(curve * (0.30F + taper_bias * 0.20F), belly_y, 0.0F);
  QVector3D const blade3(curve * 0.82F, tip_start_dist, 0.0F);
  QVector3D const blade4(curve, l, 0.0F);
  QVector3D const guard_left(-config.guard_half_width, config.guard_curve, 0.0F);
  QVector3D const guard_right(config.guard_half_width, config.guard_curve, 0.0F);

  RenderArchetypeBuilder builder{"sword_" + std::to_string(key.sword_length_key) + "_" +
                                 std::to_string(key.sword_width_key) + "_" +
                                 std::to_string(key.guard_half_width_key) + "_" +
                                 std::to_string(key.handle_radius_key) + "_" +
                                 std::to_string(key.pommel_radius_key) + "_" +
                                 std::to_string(key.pommel_length_key) + "_" +
                                 std::to_string(key.blade_ricasso_key) + "_" +
                                 std::to_string(key.blade_taper_bias_key) + "_" +
                                 std::to_string(key.blade_mid_width_scale_key) + "_" +
                                 std::to_string(key.blade_tip_width_scale_key) + "_" +
                                 std::to_string(key.blade_curve_key) + "_" +
                                 std::to_string(key.guard_curve_key) + "_" +
                                 std::to_string(key.guard_spike_length_key) + "_" +
                                 std::to_string(key.material_id)};

  builder.add_palette_mesh(get_unit_cylinder(),
                           cylinder_between(QVector3D(0.0F, -handle_len, 0.0F),
                                            QVector3D(0.0F, 0.0F, 0.0F),
                                            config.handle_radius),
                           k_leather_slot,
                           nullptr,
                           1.0F,
                           config.material_id);

  builder.add_palette_mesh(
      get_unit_cube(),
      oriented_box_between(guard_left, guard_right, guard_depth, guard_depth * 0.9F),
      k_metal_slot,
      nullptr,
      1.0F,
      config.material_id);
  builder.add_palette_mesh(
      get_unit_cube(),
      box_local_model(QVector3D(0.0F, config.guard_curve * 0.5F, 0.0F),
                      QVector3D(base_w * 0.16F, base_w * 0.14F, guard_depth * 1.2F)),
      k_metal_dark_slot,
      nullptr,
      1.0F,
      config.material_id);

  if (config.guard_spike_length > 1e-4F) {
    QVector3D const left_tip =
        guard_left +
        QVector3D(-config.guard_spike_length, config.guard_curve * 0.30F + 0.01F, 0.0F);
    QVector3D const right_tip =
        guard_right +
        QVector3D(config.guard_spike_length, config.guard_curve * 0.30F + 0.01F, 0.0F);
    builder.add_palette_mesh(
        get_unit_cone(),
        Render::Geom::cone_from_to(guard_left, left_tip, guard_end_r),
        k_metal_slot,
        nullptr,
        1.0F,
        config.material_id);
    builder.add_palette_mesh(
        get_unit_cone(),
        Render::Geom::cone_from_to(guard_right, right_tip, guard_end_r),
        k_metal_slot,
        nullptr,
        1.0F,
        config.material_id);
  }

  auto add_blade_segment = [&](const QVector3D& start,
                               const QVector3D& end,
                               float half_width,
                               float depth_scale,
                               std::uint8_t slot) {
    builder.add_palette_mesh(get_unit_cube(),
                             oriented_box_between(start,
                                                  end,
                                                  half_width,
                                                  blade_thickness * depth_scale,
                                                  QVector3D(1.0F, 0.0F, 0.0F)),
                             slot,
                             nullptr,
                             1.0F,
                             config.material_id);
  };

  add_blade_segment(blade0, blade1, base_half_w, 1.00F, k_metal_slot);
  add_blade_segment(blade1, blade2, mid_half_w, 0.96F, k_metal_slot);
  add_blade_segment(blade2, blade3, upper_half_w, 0.86F, k_metal_slot);

  builder.add_palette_mesh(get_unit_cube(),
                           oriented_box_between(blade1 + QVector3D(0.0F, 0.01F, 0.0F),
                                                blade2,
                                                base_half_w * 0.20F,
                                                blade_thickness * 0.55F,
                                                QVector3D(1.0F, 0.0F, 0.0F)),
                           k_fuller_slot,
                           nullptr,
                           1.0F,
                           config.material_id);
  builder.add_palette_mesh(get_unit_cube(),
                           oriented_box_between(blade2,
                                                blade3 - QVector3D(0.0F, 0.02F, 0.0F),
                                                upper_half_w * 0.18F,
                                                blade_thickness * 0.50F,
                                                QVector3D(1.0F, 0.0F, 0.0F)),
                           k_metal_dark_slot,
                           nullptr,
                           1.0F,
                           config.material_id);

  builder.add_palette_mesh(get_unit_cone(),
                           Render::Geom::cone_from_to(blade3, blade4, tip_half_w),
                           k_metal_slot,
                           nullptr,
                           1.0F,
                           config.material_id);

  builder.add_palette_mesh(get_unit_cube(),
                           box_local_model(QVector3D(0.0F, pommel_y, 0.0F),
                                           QVector3D(config.pommel_radius * 0.85F,
                                                     config.pommel_radius * 0.70F,
                                                     config.pommel_radius * 0.85F)),
                           k_metal_slot,
                           nullptr,
                           1.0F,
                           config.material_id);
  if (config.pommel_length > 1e-4F) {
    builder.add_palette_mesh(
        get_unit_cone(),
        Render::Geom::cone_from_to(
            QVector3D(0.0F, pommel_y - config.pommel_radius * 0.35F, 0.0F),
            QVector3D(0.0F,
                      pommel_y - config.pommel_radius * 0.35F - config.pommel_length,
                      0.0F),
            config.pommel_radius * 0.65F),
        k_metal_dark_slot,
        nullptr,
        1.0F,
        config.material_id);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

enum ScabbardPaletteSlot : std::uint8_t {
  k_scabbard_leather_slot = 0U,
  k_scabbard_metal_slot = 1U,
};

struct ScabbardKey {
  int sheath_r_key{0};
};

auto operator==(const ScabbardKey& a, const ScabbardKey& b) -> bool {
  return a.sheath_r_key == b.sheath_r_key;
}

auto scabbard_archetype(float sheath_r) -> const RenderArchetype& {
  struct CachedArchetype {
    ScabbardKey key;
    RenderArchetype archetype;
  };
  static std::deque<CachedArchetype> cache;
  ScabbardKey const key{static_cast<int>(std::lround(sheath_r * 1000.0F))};
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  QVector3D const tip(-0.05F, -0.22F, -0.12F);
  QVector3D const metal_tip = tip + QVector3D(-0.02F, -0.02F, -0.02F);
  RenderArchetypeBuilder builder{"scabbard_" + std::to_string(key.sheath_r_key)};
  builder.add_palette_mesh(get_unit_cylinder(),
                           cylinder_between(QVector3D(0.0F, 0.0F, 0.0F), tip, sheath_r),
                           k_scabbard_leather_slot,
                           nullptr,
                           1.0F,
                           0);
  builder.add_palette_mesh(get_unit_cone(),
                           Render::Geom::cone_from_to(tip, metal_tip, sheath_r),
                           k_scabbard_metal_slot,
                           nullptr,
                           1.0F,
                           0);
  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

SwordRenderer::SwordRenderer(SwordRenderConfig config)
    : m_base(config) {
}

void SwordRenderer::render(const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void SwordRenderer::submit(const SwordRenderConfig& m_config,
                           const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  bool const is_attacking = anim.inputs.is_attacking && anim.inputs.is_melee;
  float attack_phase = 0.0F;
  if (is_attacking) {
    bool const use_authored_phase = anim.inputs.has_attack_offset ||
                                    anim.inputs.combat_phase != CombatAnimPhase::Idle ||
                                    anim.amplified_attack ||
                                    anim.inputs.finisher_attack;
    attack_phase =
        use_authored_phase
            ? clamp01(anim.attack_phase)
            : std::fmod(anim.inputs.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  // Per-direction sword arc keyframes for visually distinct attack types
  // Each direction has: ready position, wind-up apex, strike end, return pos
  QVector3D ready_pos(0.10F, 0.85F, 0.52F);    // Default rest: sword up-right
  QVector3D windup_pos(0.0F, 1.0F, 0.0F);      // Wind-up: raised high
  QVector3D strike_mid(0.0F, 0.50F, 0.87F);    // Mid-strike: forward/down
  QVector3D strike_end(-0.08F, -0.10F, 0.99F); // Strike end: fully extended low

  std::uint8_t const variant = anim.inputs.attack_variant;
  if (anim.amplified_attack) {
    switch (variant) {
    case 0: // LeftSlash - wide arc from upper-right to lower-left
      ready_pos = QVector3D(0.45F, 0.85F, 0.28F);
      windup_pos = QVector3D(0.70F, 0.65F, 0.30F);
      strike_mid = QVector3D(-0.20F, 0.30F, 0.93F);
      strike_end = QVector3D(-0.72F, -0.15F, 0.68F);
      break;
    case 1: // RightSlash - wide arc from upper-left to lower-right
      ready_pos = QVector3D(-0.40F, 0.88F, 0.25F);
      windup_pos = QVector3D(-0.68F, 0.68F, 0.28F);
      strike_mid = QVector3D(0.22F, 0.28F, 0.93F);
      strike_end = QVector3D(0.74F, -0.12F, 0.67F);
      break;
    case 2: // Finisher / HeavyOverhead - high up then straight down
      ready_pos = QVector3D(0.0F, 0.90F, 0.44F);
      windup_pos = QVector3D(0.0F, 1.0F, -0.05F);
      strike_mid = QVector3D(0.0F, 0.40F, 0.92F);
      strike_end = QVector3D(0.0F, -0.30F, 0.95F);
      break;
    case 3: // Overhead - angled overhead from right
      ready_pos = QVector3D(0.20F, 0.92F, 0.34F);
      windup_pos = QVector3D(0.25F, 1.0F, -0.10F);
      strike_mid = QVector3D(0.05F, 0.35F, 0.94F);
      strike_end = QVector3D(-0.10F, -0.20F, 0.98F);
      break;
    case 4: // Thrust - straight forward lunge
      ready_pos = QVector3D(0.15F, 0.55F, 0.82F);
      windup_pos = QVector3D(0.10F, 0.60F, -0.20F);
      strike_mid = QVector3D(0.05F, 0.15F, 0.99F);
      strike_end = QVector3D(0.02F, 0.05F, 1.0F);
      break;
    default: // Alternating slash fallback
      if (variant % 2 == 0) {
        ready_pos = QVector3D(0.38F, 0.88F, 0.28F);
        windup_pos = QVector3D(0.60F, 0.72F, 0.34F);
        strike_mid = QVector3D(-0.15F, 0.30F, 0.94F);
        strike_end = QVector3D(-0.65F, -0.10F, 0.76F);
      } else {
        ready_pos = QVector3D(-0.35F, 0.90F, 0.26F);
        windup_pos = QVector3D(-0.58F, 0.74F, 0.34F);
        strike_mid = QVector3D(0.18F, 0.30F, 0.93F);
        strike_end = QVector3D(0.68F, -0.08F, 0.73F);
      }
      break;
    }
  }

  // Normalize all direction vectors
  auto safe_normalize = [](QVector3D& v) {
    if (v.lengthSquared() > 1e-6F) {
      v.normalize();
    }
  };
  safe_normalize(ready_pos);
  safe_normalize(windup_pos);
  safe_normalize(strike_mid);
  safe_normalize(strike_end);

  QVector3D sword_dir = ready_pos;

  if (is_attacking) {
    // Phase timeline for full, dramatic swing:
    // 0.00 - 0.20: Wind-up (ready → windup apex, slow gather)
    // 0.20 - 0.30: Hold at apex (tension/anticipation)
    // 0.30 - 0.52: Strike swing (windup → strike_mid → strike_end, fast & powerful)
    // 0.52 - 0.65: Follow-through (hold near strike_end, weight)
    // 0.65 - 0.85: Recovery arc (strike_end back toward ready_pos)
    // 0.85 - 1.00: Settle (easing back to rest)
    if (attack_phase < 0.20F) {
      // Wind-up: slow rising motion to gathered position
      float const t = ease_in_out_cubic(attack_phase / 0.20F);
      sword_dir = nlerp(ready_pos, windup_pos, t);
    } else if (attack_phase < 0.30F) {
      // Apex hold: slight tremor at peak for tension
      float const hold_t = (attack_phase - 0.20F) / 0.10F;
      float const tremor = std::sin(hold_t * 12.0F) * 0.02F;
      sword_dir = windup_pos + QVector3D(tremor, tremor * 0.5F, 0.0F);
      safe_normalize(sword_dir);
    } else if (attack_phase < 0.40F) {
      // Strike first half: explosive start, windup → strike_mid
      float t = (attack_phase - 0.30F) / 0.10F;
      t = t * t; // Accelerating (ease-in)
      sword_dir = nlerp(windup_pos, strike_mid, t);
    } else if (attack_phase < 0.52F) {
      // Strike second half: strike_mid → strike_end, fastest part
      float t = (attack_phase - 0.40F) / 0.12F;
      t = 1.0F - (1.0F - t) * (1.0F - t); // Decelerating (ease-out)
      sword_dir = nlerp(strike_mid, strike_end, t);
    } else if (attack_phase < 0.65F) {
      // Follow-through: hold near strike_end with slight drift
      float const drift = (attack_phase - 0.52F) / 0.13F;
      QVector3D const drift_target = nlerp(strike_end, ready_pos, 0.08F);
      sword_dir = nlerp(strike_end, drift_target, drift);
    } else if (attack_phase < 0.85F) {
      // Recovery arc: sweeping back toward ready position
      float const t = ease_in_out_cubic((attack_phase - 0.65F) / 0.20F);
      sword_dir = nlerp(strike_end, ready_pos, t);
    } else {
      // Settle: ease into rest
      float const t = ease_in_out_cubic((attack_phase - 0.85F) / 0.15F);
      QVector3D const almost_ready =
          nlerp(ready_pos, QVector3D(0.08F, 0.92F, 0.38F), 0.1F);
      sword_dir = nlerp(almost_ready, ready_pos, t);
      safe_normalize(sword_dir);
    }
  }

  std::array<QVector3D, 4> const palette_slots{m_config.metal_color,
                                               m_config.metal_color * 0.92F,
                                               m_config.metal_color * 0.65F,
                                               palette.leather};
  AttachmentFrame const grip =
      frames.hand_r.radius > 0.0F
          ? Render::Humanoid::socket_attachment_frame(
                frames.hand_r, Render::Humanoid::HumanoidSocket::GripR)
          : (frames.grip_r.radius > 0.0F
                 ? frames.grip_r
                 : Render::Humanoid::socket_attachment_frame(
                       frames.hand_r, Render::Humanoid::HumanoidSocket::GripR));
  append_equipment_archetype(batch,
                             sword_archetype(m_config),
                             hand_basis_transform(ctx.model, grip) *
                                 sword_local_pose(sword_dir),
                             palette_slots);

  if (is_attacking && attack_phase >= 0.28F && attack_phase < 0.68F) {
    float const base_w = m_config.sword_width;
    float const t = (attack_phase - 0.28F) / 0.40F;
    // Trail is bright at start, fades out toward end
    float const alpha = clamp01(0.60F * std::sin(t * std::numbers::pi_v<float>));
    QMatrix4x4 const sword_world =
        hand_basis_transform(ctx.model, grip) * sword_local_pose(sword_dir);
    QVector3D const blade_base = sword_world.column(3).toVector3D();
    QVector3D const blade_tip =
        (sword_world * QVector4D(0.0F, m_config.sword_length, 0.0F, 1.0F)).toVector3D();
    QVector3D swing_dir = blade_tip - blade_base;
    if (swing_dir.lengthSquared() > 1e-6F) {
      swing_dir.normalize();
    } else {
      swing_dir = QVector3D(0.0F, 1.0F, 0.0F);
    }
    QVector3D const guard_right = sword_world.column(0).toVector3D();
    // Trail extends along more of the blade length
    QVector3D const trail_start = blade_base + swing_dir * 0.04F;
    QVector3D const trail_end = blade_base + swing_dir * (0.55F + 0.35F * t);

    // Hot white-blue core trail (modern RPG style)
    QVector3D const hot_core_color =
        lerp(QVector3D(1.0F, 0.95F, 0.85F), m_config.metal_color * 1.3F, t);
    std::array<QVector3D, 1> const core_palette{hot_core_color};
    append_equipment_archetype(
        batch,
        single_cone_archetype(base_w * 0.8F, m_config.material_id, "sword_trail"),
        oriented_segment_transform(
            ctx.model, trail_start, trail_end - trail_start, guard_right),
        core_palette,
        nullptr,
        alpha * 0.85F);

    // Outer glow trail (wider, colored)
    QVector3D const glow_color =
        lerp(QVector3D(0.7F, 0.85F, 1.0F), m_config.metal_color * 0.8F, t * 0.7F);
    std::array<QVector3D, 1> const glow_palette{glow_color};
    append_equipment_archetype(
        batch,
        single_cone_archetype(base_w * 1.6F, m_config.material_id, "sword_trail"),
        oriented_segment_transform(
            ctx.model, trail_start, trail_end - trail_start, guard_right),
        glow_palette,
        nullptr,
        alpha * 0.45F);

    // Secondary, wider ghost trail for afterimage effect
    if (attack_phase >= 0.32F && attack_phase < 0.60F) {
      float const ghost_t = (attack_phase - 0.32F) / 0.28F;
      float const ghost_alpha = clamp01(0.28F * (1.0F - ghost_t * ghost_t));
      QVector3D const ghost_end = blade_base + swing_dir * (0.42F + 0.22F * ghost_t);
      QVector3D const ghost_color =
          lerp(QVector3D(0.5F, 0.6F, 0.9F), m_config.metal_color * 0.5F, ghost_t);
      std::array<QVector3D, 1> const ghost_palette{ghost_color};
      append_equipment_archetype(
          batch,
          single_cone_archetype(base_w * 2.4F, m_config.material_id, "sword_trail"),
          oriented_segment_transform(
              ctx.model, trail_start, ghost_end - trail_start, guard_right),
          ghost_palette,
          nullptr,
          ghost_alpha);
    }
  }

  // Speed-line afterimage: render ghost swords at previous positions during peak strike
  if (is_attacking && anim.amplified_attack && attack_phase >= 0.32F &&
      attack_phase < 0.54F) {
    // Render 2 afterimage swords at slightly earlier phases for motion blur effect
    constexpr int k_afterimage_count = 2;
    constexpr float k_phase_offsets[k_afterimage_count] = {0.04F, 0.09F};
    constexpr float k_afterimage_alphas[k_afterimage_count] = {0.22F, 0.10F};

    for (int ai = 0; ai < k_afterimage_count; ++ai) {
      float const prev_phase = attack_phase - k_phase_offsets[ai];
      if (prev_phase < 0.28F) {
        continue;
      }
      // Recalculate sword direction at previous phase
      QVector3D prev_dir;
      if (prev_phase < 0.40F) {
        float pt = (prev_phase - 0.30F) / 0.10F;
        pt = pt * pt;
        prev_dir = nlerp(windup_pos, strike_mid, pt);
      } else {
        float pt = (prev_phase - 0.40F) / 0.12F;
        pt = 1.0F - (1.0F - pt) * (1.0F - pt);
        prev_dir = nlerp(strike_mid, strike_end, pt);
      }

      std::array<QVector3D, 4> const ghost_sword_palette{m_config.metal_color * 0.7F,
                                                         m_config.metal_color * 0.6F,
                                                         m_config.metal_color * 0.4F,
                                                         palette.leather * 0.5F};
      append_equipment_archetype(batch,
                                 sword_archetype(m_config),
                                 hand_basis_transform(ctx.model, grip) *
                                     sword_local_pose(prev_dir),
                                 ghost_sword_palette,
                                 nullptr,
                                 k_afterimage_alphas[ai]);
    }
  }
}

auto sword_fill_role_colors(const HumanoidPalette& palette,
                            const SwordRenderConfig&,
                            QVector3D* out,
                            std::size_t max) -> std::uint32_t {
  if (max < k_sword_role_count) {
    return 0U;
  }
  out[k_metal_slot] = palette.metal;
  out[k_metal_dark_slot] = palette.metal * 0.92F;
  out[k_fuller_slot] = palette.metal * 0.65F;
  out[k_leather_slot] = palette.leather;
  return k_sword_role_count;
}

auto sword_make_static_attachment(const SwordRenderConfig& config,
                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  return sword_make_static_attachment(
      config, base_role_byte, QVector3D(0.02F, 0.97F, 0.0F));
}

auto sword_make_static_attachment(const SwordRenderConfig& config,
                                  std::uint8_t base_role_byte,
                                  const QVector3D& blade_dir_local)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  auto const& bind_grip = Render::Humanoid::humanoid_bind_body_frames().grip_r;
  QMatrix4x4 const bind_socket = hand_basis_transform(QMatrix4x4{}, bind_grip);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &sword_archetype(config),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = sword_local_pose(blade_dir_local),
  });
  spec.palette_role_remap[k_metal_slot] = base_role_byte;
  spec.palette_role_remap[k_metal_dark_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_fuller_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_leather_slot] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  return spec;
}

auto scabbard_fill_role_colors(const HumanoidPalette& palette,
                               QVector3D* out,
                               std::size_t max) -> std::uint32_t {
  if (max < k_scabbard_role_count) {
    return 0U;
  }
  out[k_scabbard_leather_slot] = palette.leather * 0.9F;
  out[k_scabbard_metal_slot] = palette.metal;
  return k_scabbard_role_count;
}

auto scabbard_make_static_attachment(float sheath_r,
                                     std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  using HP = HumanProportions;
  QMatrix4x4 pose;
  pose.translate(QVector3D(0.10F, HP::WAIST_Y - 0.04F, -0.02F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &scabbard_archetype(sheath_r),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = pose,
  });
  spec.palette_role_remap[k_scabbard_leather_slot] = base_role_byte;
  spec.palette_role_remap[k_scabbard_metal_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

} // namespace Render::GL
