#include "sword_renderer.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/style_palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "../oriented_archetype_utils.h"
#include "../primitive_archetype_utils.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

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
  int blade_ricasso_key{0};
  int material_id{0};
};

auto operator==(const SwordArchetypeKey &lhs,
                const SwordArchetypeKey &rhs) -> bool {
  return lhs.sword_length_key == rhs.sword_length_key &&
         lhs.sword_width_key == rhs.sword_width_key &&
         lhs.guard_half_width_key == rhs.guard_half_width_key &&
         lhs.handle_radius_key == rhs.handle_radius_key &&
         lhs.pommel_radius_key == rhs.pommel_radius_key &&
         lhs.blade_ricasso_key == rhs.blade_ricasso_key &&
         lhs.material_id == rhs.material_id;
}

auto quantize_sword_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

auto sword_basis_transform(const QMatrix4x4 &parent, const QVector3D &origin,
                           const QVector3D &blade_axis,
                           const QVector3D &guard_axis) -> QMatrix4x4 {
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

auto sword_archetype(const SwordRenderConfig &config)
    -> const RenderArchetype & {
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
                              quantize_sword_value(config.blade_ricasso),
                              config.material_id};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const l = config.sword_length;
  float const base_w = config.sword_width;
  float const blade_thickness = base_w * 0.15F;
  float const ricasso_len = clamp_f(config.blade_ricasso, 0.10F, l * 0.30F);
  float const mid_w = base_w * 0.95F;
  float const tip_start_dist = lerp(ricasso_len, l, 0.70F);

  RenderArchetypeBuilder builder{"sword_" +
                                 std::to_string(key.sword_length_key) + "_" +
                                 std::to_string(key.sword_width_key) + "_" +
                                 std::to_string(key.guard_half_width_key) +
                                 "_" + std::to_string(key.handle_radius_key) +
                                 "_" + std::to_string(key.pommel_radius_key) +
                                 "_" + std::to_string(key.blade_ricasso_key) +
                                 "_" + std::to_string(key.material_id)};

  builder.add_palette_mesh(get_unit_cylinder(),
                           cylinder_between(QVector3D(0.0F, -0.10F, 0.0F),
                                            QVector3D(0.0F, 0.0F, 0.0F),
                                            config.handle_radius),
                           k_leather_slot, nullptr, 1.0F, config.material_id);

  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(QVector3D(-config.guard_half_width, 0.0F, 0.0F),
                       QVector3D(config.guard_half_width, 0.0F, 0.0F), 0.014F),
      k_metal_slot, nullptr, 1.0F, config.material_id);
  builder.add_palette_mesh(
      get_unit_sphere(),
      sphere_at(QVector3D(-config.guard_half_width, 0.0F, 0.0F), 0.018F),
      k_metal_slot, nullptr, 1.0F, config.material_id);
  builder.add_palette_mesh(
      get_unit_sphere(),
      sphere_at(QVector3D(config.guard_half_width, 0.0F, 0.0F), 0.018F),
      k_metal_slot, nullptr, 1.0F, config.material_id);

  auto add_flat_section = [&](float start_y, float end_y, float width) {
    float const offset = width * 0.33F;
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(QVector3D(0.0F, start_y, 0.0F),
                                              QVector3D(0.0F, end_y, 0.0F),
                                              blade_thickness),
                             k_metal_slot, nullptr, 1.0F, config.material_id);
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(QVector3D(offset, start_y, 0.0F),
                                              QVector3D(offset, end_y, 0.0F),
                                              blade_thickness * 0.8F),
                             k_metal_dark_slot, nullptr, 1.0F,
                             config.material_id);
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(QVector3D(-offset, start_y, 0.0F),
                                              QVector3D(-offset, end_y, 0.0F),
                                              blade_thickness * 0.8F),
                             k_metal_dark_slot, nullptr, 1.0F,
                             config.material_id);
  };

  add_flat_section(0.0F, ricasso_len, base_w);
  add_flat_section(ricasso_len, tip_start_dist, mid_w);

  constexpr int k_tip_segments = 3;
  for (int i = 0; i < k_tip_segments; ++i) {
    float const t0 = static_cast<float>(i) / static_cast<float>(k_tip_segments);
    float const t1 =
        static_cast<float>(i + 1) / static_cast<float>(k_tip_segments);
    float const seg_start_y = tip_start_dist + (l - tip_start_dist) * t0;
    float const seg_end_y = tip_start_dist + (l - tip_start_dist) * t1;
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(QVector3D(0.0F, seg_start_y, 0.0F),
                         QVector3D(0.0F, seg_end_y, 0.0F), blade_thickness),
        k_metal_slot, nullptr, 1.0F, config.material_id);
  }

  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(QVector3D(0.0F, ricasso_len + 0.02F, 0.0F),
                       QVector3D(0.0F, tip_start_dist - 0.06F, 0.0F),
                       blade_thickness * 0.6F),
      k_fuller_slot, nullptr, 1.0F, config.material_id);

  builder.add_palette_mesh(
      get_unit_sphere(),
      sphere_at(QVector3D(0.0F, -0.12F, 0.0F), config.pommel_radius),
      k_metal_slot, nullptr, 1.0F, config.material_id);

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

auto operator==(const ScabbardKey &a, const ScabbardKey &b) -> bool {
  return a.sheath_r_key == b.sheath_r_key;
}

auto scabbard_archetype(float sheath_r) -> const RenderArchetype & {
  struct CachedArchetype {
    ScabbardKey key;
    RenderArchetype archetype;
  };
  static std::deque<CachedArchetype> cache;
  ScabbardKey const key{static_cast<int>(std::lround(sheath_r * 1000.0F))};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  QVector3D const tip(-0.05F, -0.22F, -0.12F);
  QVector3D const metal_tip = tip + QVector3D(-0.02F, -0.02F, -0.02F);
  RenderArchetypeBuilder builder{"scabbard_" +
                                 std::to_string(key.sheath_r_key)};
  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(QVector3D(0.0F, 0.0F, 0.0F), tip, sheath_r),
      k_scabbard_leather_slot, nullptr, 1.0F, 0);
  builder.add_palette_mesh(get_unit_cone(),
                           Render::Geom::cone_from_to(tip, metal_tip, sheath_r),
                           k_scabbard_metal_slot, nullptr, 1.0F, 0);
  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

SwordRenderer::SwordRenderer(SwordRenderConfig config)
    : m_base(std::move(config)) {}

void SwordRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void SwordRenderer::submit(const SwordRenderConfig &m_config,
                           const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  QVector3D const grip_pos = frames.hand_r.origin;

  bool const is_attacking = anim.inputs.is_attacking && anim.inputs.is_melee;
  float attack_phase = 0.0F;
  if (is_attacking) {
    attack_phase =
        std::fmod(anim.inputs.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  constexpr float k_sword_yaw_deg = 25.0F;
  QMatrix4x4 yaw_m;
  yaw_m.rotate(k_sword_yaw_deg, 0.0F, 1.0F, 0.0F);

  QVector3D upish = yaw_m.map(QVector3D(0.05F, 1.0F, 0.15F));
  QVector3D midish = yaw_m.map(QVector3D(0.08F, 0.20F, 1.0F));
  QVector3D downish = yaw_m.map(QVector3D(0.10F, -1.0F, 0.25F));
  if (upish.lengthSquared() > 1e-6F) {
    upish.normalize();
  }
  if (midish.lengthSquared() > 1e-6F) {
    midish.normalize();
  }
  if (downish.lengthSquared() > 1e-6F) {
    downish.normalize();
  }

  QVector3D sword_dir = upish;

  if (is_attacking) {
    if (attack_phase < 0.18F) {
      float const t = ease_in_out_cubic(attack_phase / 0.18F);
      sword_dir = nlerp(upish, upish, t);
    } else if (attack_phase < 0.32F) {
      float const t = ease_in_out_cubic((attack_phase - 0.18F) / 0.14F);
      sword_dir = nlerp(upish, midish, t * 0.35F);
    } else if (attack_phase < 0.52F) {
      float t = (attack_phase - 0.32F) / 0.20F;
      t = t * t * t;
      if (t < 0.5F) {
        float const u = t / 0.5F;
        sword_dir = nlerp(upish, midish, u);
      } else {
        float const u = (t - 0.5F) / 0.5F;
        sword_dir = nlerp(midish, downish, u);
      }
    } else if (attack_phase < 0.72F) {
      float const t = ease_in_out_cubic((attack_phase - 0.52F) / 0.20F);
      sword_dir = nlerp(downish, midish, t);
    } else {
      float const t = smoothstep(0.72F, 1.0F, attack_phase);
      sword_dir = nlerp(midish, upish, t);
    }
  }

  QVector3D const blade_base = grip_pos;

  QVector3D const guard_center = blade_base;
  float const gw = m_config.guard_half_width;

  QVector3D guard_right =
      QVector3D::crossProduct(QVector3D(0, 1, 0), sword_dir);
  if (guard_right.lengthSquared() < 1e-6F) {
    guard_right = QVector3D::crossProduct(QVector3D(1, 0, 0), sword_dir);
  }
  guard_right.normalize();

  std::array<QVector3D, 4> const palette_slots{
      m_config.metal_color, m_config.metal_color * 0.92F,
      m_config.metal_color * 0.65F, palette.leather};
  append_equipment_archetype(
      batch, sword_archetype(m_config),
      sword_basis_transform(ctx.model, guard_center, sword_dir, guard_right),
      palette_slots);

  if (is_attacking && attack_phase >= 0.32F && attack_phase < 0.56F) {
    float const base_w = m_config.sword_width;
    float const t = (attack_phase - 0.32F) / 0.24F;
    float const alpha = clamp01(0.35F * (1.0F - t));
    QVector3D const trail_start = blade_base - sword_dir * 0.05F;
    QVector3D const trail_end = blade_base - sword_dir * (0.28F + 0.15F * t);
    std::array<QVector3D, 1> const trail_palette{m_config.metal_color * 0.9F};
    append_equipment_archetype(
        batch,
        single_cone_archetype(base_w * 0.9F, m_config.material_id,
                              "sword_trail"),
        oriented_segment_transform(ctx.model, trail_end,
                                   trail_start - trail_end, guard_right),
        trail_palette, nullptr, alpha);
  }
}

auto sword_fill_role_colors(const HumanoidPalette &palette,
                            const SwordRenderConfig &, QVector3D *out,
                            std::size_t max) -> std::uint32_t {
  if (max < kSwordRoleCount) {
    return 0U;
  }
  out[k_metal_slot] = palette.metal;
  out[k_metal_dark_slot] = palette.metal * 0.92F;
  out[k_fuller_slot] = palette.metal * 0.65F;
  out[k_leather_slot] = palette.leather;
  return kSwordRoleCount;
}

auto sword_make_static_attachment(const SwordRenderConfig &config,
                                  std::uint16_t socket_bone_index,
                                  std::uint8_t base_role_byte,
                                  const QMatrix4x4 &bind_hand_r_matrix)
    -> Render::Creature::StaticAttachmentSpec {
  QVector3D const origin = bind_hand_r_matrix.column(3).toVector3D();

  constexpr float k_sword_yaw_deg = 25.0F;
  QMatrix4x4 yaw_m;
  yaw_m.rotate(k_sword_yaw_deg, 0.0F, 1.0F, 0.0F);
  QVector3D blade_dir = yaw_m.map(QVector3D(0.05F, 1.0F, 0.15F));
  if (blade_dir.lengthSquared() > 1e-6F) {
    blade_dir.normalize();
  }
  QVector3D guard_right =
      QVector3D::crossProduct(QVector3D(0.0F, 1.0F, 0.0F), blade_dir);
  if (guard_right.lengthSquared() < 1e-6F) {
    guard_right =
        QVector3D::crossProduct(QVector3D(1.0F, 0.0F, 0.0F), blade_dir);
  }
  if (guard_right.lengthSquared() > 1e-6F) {
    guard_right.normalize();
  }
  QVector3D z_axis = QVector3D::crossProduct(guard_right, blade_dir);
  if (z_axis.lengthSquared() > 1e-6F) {
    z_axis.normalize();
  }

  QMatrix4x4 sword_pose;
  sword_pose.setColumn(0, QVector4D(guard_right, 0.0F));
  sword_pose.setColumn(1, QVector4D(blade_dir, 0.0F));
  sword_pose.setColumn(2, QVector4D(z_axis, 0.0F));
  sword_pose.setColumn(3, QVector4D(origin, 1.0F));

  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &sword_archetype(config),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = sword_pose,
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

auto scabbard_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                               std::size_t max) -> std::uint32_t {
  if (max < kScabbardRoleCount) {
    return 0U;
  }
  out[k_scabbard_leather_slot] = palette.leather * 0.9F;
  out[k_scabbard_metal_slot] = palette.metal;
  return kScabbardRoleCount;
}

auto scabbard_make_static_attachment(
    float sheath_r, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec {
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
