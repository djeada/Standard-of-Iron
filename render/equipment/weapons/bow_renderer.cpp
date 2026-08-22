#include "bow_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

#include "animation/rig/humanoid_proportions.h"
#include "arrow_archetype_utils.h"
#include "math/math_utils.h"
#include "render/entity/registry.h"
#include "render/entity/renderer_constants.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/oriented_archetype_utils.h"
#include "render/equipment/primitive_archetype_utils.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/static_attachment_spec.h"

namespace Render::GL {

using Render::Geom::clamp_f;

namespace {
constexpr QVector3D k_dark_bow_color(0.22F, 0.145F, 0.085F);
constexpr float k_bow_length_scale = 1.05F;
constexpr float k_bow_thickness_scale = 1.0F;
constexpr float k_bow_depth_scale = 1.0F;
constexpr float k_bow_curve_boost = 1.0F;
constexpr float k_bow_grip_side_offset = 0.0F;
constexpr float k_bow_brace_scale = 0.6F;
constexpr float k_bow_brace_min = 0.10F;
constexpr float k_bow_brace_max = 0.20F;
constexpr float k_nocked_arrow_length = 0.78F;
constexpr float k_nocked_arrow_shaft_radius = 0.011F;
constexpr float k_nocked_arrow_head_radius = 0.024F;
constexpr float k_nocked_arrow_head_length = 0.075F;
constexpr float k_nocked_arrow_fletching_radius = 0.026F;
constexpr float k_nocked_arrow_fletching_length = 0.055F;
constexpr float k_nocked_arrow_rest_height = 0.03F;

enum BowPaletteSlot : std::uint8_t {
  k_bow_body_slot = 0U,
  k_bow_string_slot = 1U,
  k_bow_arrow_shaft_role = 2U,
  k_bow_arrow_head_role = 3U,
  k_bow_arrow_fletching_role = 4U,
};

struct BowResolvedGeometry {
  float rod_radius{0.0F};
  float string_radius{0.0F};
  float depth{0.0F};
  float half_height{0.0F};
  float curve_factor{0.0F};
  float string_setback{0.0F};
  float attack_string_radius{0.0F};
  float max_draw_depth{0.0F};
};

struct BowPlane {
  QVector3D top_end;
  QVector3D bot_end;
  QVector3D control;
  QVector3D string_center;
};

struct BowBodyKey {
  int rod_radius_key{0};
  int bow_depth_key{0};
  int bow_x_key{0};
  int bow_forward_offset_key{0};
  int bow_top_y_key{0};
  int bow_bot_y_key{0};
  int bow_height_scale_key{0};
  int bow_curve_factor_key{0};
  int material_id{0};
};

auto operator==(const BowBodyKey& lhs, const BowBodyKey& rhs) -> bool {
  return lhs.rod_radius_key == rhs.rod_radius_key &&
         lhs.bow_depth_key == rhs.bow_depth_key && lhs.bow_x_key == rhs.bow_x_key &&
         lhs.bow_forward_offset_key == rhs.bow_forward_offset_key &&
         lhs.bow_top_y_key == rhs.bow_top_y_key &&
         lhs.bow_bot_y_key == rhs.bow_bot_y_key &&
         lhs.bow_height_scale_key == rhs.bow_height_scale_key &&
         lhs.bow_curve_factor_key == rhs.bow_curve_factor_key &&
         lhs.material_id == rhs.material_id;
}

auto quantize_bow_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

auto resolve_bow_geometry(const BowRenderConfig& config) -> BowResolvedGeometry {
  BowResolvedGeometry geometry;
  geometry.rod_radius = config.bow_rod_radius * k_bow_thickness_scale;
  geometry.string_radius = config.string_radius * k_bow_thickness_scale;
  geometry.depth = config.bow_depth * k_bow_depth_scale;
  geometry.half_height = (config.bow_top_y - config.bow_bot_y) * 0.5F *
                         config.bow_height_scale * k_bow_length_scale;
  geometry.curve_factor = config.bow_curve_factor * k_bow_curve_boost;
  geometry.string_setback =
      std::clamp(geometry.depth * geometry.curve_factor * k_bow_brace_scale,
                 k_bow_brace_min,
                 k_bow_brace_max);
  geometry.attack_string_radius = geometry.string_radius * 0.5625F;
  geometry.max_draw_depth = std::max(0.35F, geometry.depth * 1.2F);
  return geometry;
}

auto resolve_bow_plane(const BowRenderConfig& config,
                       const BowResolvedGeometry& geometry) -> BowPlane {
  float const plane_x = config.bow_x + k_bow_grip_side_offset;
  float const grip_z = config.bow_forward_offset;
  float const brace = geometry.string_setback;

  BowPlane plane;
  plane.top_end = QVector3D(plane_x, geometry.half_height, grip_z - brace);
  plane.bot_end = QVector3D(plane_x, -geometry.half_height, grip_z - brace);

  plane.control = QVector3D(plane_x, 0.0F, grip_z + brace);
  plane.string_center = QVector3D(plane_x, 0.0F, grip_z - brace);
  return plane;
}

auto bow_body_archetype(const BowRenderConfig& config) -> const RenderArchetype& {
  struct CachedArchetype {
    BowBodyKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  BowResolvedGeometry const geometry = resolve_bow_geometry(config);
  BowBodyKey const key{
      quantize_bow_value(geometry.rod_radius),
      quantize_bow_value(geometry.depth),
      quantize_bow_value(config.bow_x),
      quantize_bow_value(config.bow_forward_offset),
      quantize_bow_value(geometry.half_height),
      0,
      quantize_bow_value(k_bow_length_scale),
      quantize_bow_value(geometry.curve_factor),
      config.material_id,
  };
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  BowPlane const plane = resolve_bow_plane(config, geometry);
  QVector3D const top_end = plane.top_end;
  QVector3D const bot_end = plane.bot_end;
  QVector3D const ctrl = plane.control;

  auto q_bezier =
      [](const QVector3D& a, const QVector3D& c, const QVector3D& b, float t) {
        float const u = 1.0F - t;
        return u * u * a + 2.0F * u * t * c + t * t * b;
      };

  constexpr int k_bow_segments = 22;
  RenderArchetypeBuilder builder{
      "bow_body_" + std::to_string(key.rod_radius_key) + "_" +
      std::to_string(key.bow_depth_key) + "_" + std::to_string(key.bow_x_key) + "_" +
      std::to_string(key.bow_forward_offset_key) + "_" +
      std::to_string(key.bow_top_y_key) + "_" + std::to_string(key.bow_bot_y_key) +
      "_" + std::to_string(key.bow_height_scale_key) + "_" +
      std::to_string(key.bow_curve_factor_key) + "_" + std::to_string(key.material_id)};

  QVector3D prev = bot_end;
  for (int i = 1; i <= k_bow_segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_bow_segments);
    QVector3D const cur = q_bezier(bot_end, ctrl, top_end, t);
    add_generated_equipment_primitive(
        builder,
        generated_cylinder(
            prev, cur, geometry.rod_radius, k_bow_body_slot, 1.0F, config.material_id));
    prev = cur;
  }

  QVector3D const grip_center(plane.top_end.x(), 0.0F, config.bow_forward_offset);
  add_generated_equipment_primitive(
      builder,
      generated_cylinder(grip_center - QVector3D(0.0F, 0.07F, 0.0F),
                         grip_center + QVector3D(0.0F, 0.07F, 0.0F),
                         geometry.rod_radius * 1.45F,
                         k_bow_body_slot,
                         1.0F,
                         config.material_id));

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto bow_string_archetype(const BowRenderConfig& config) -> const RenderArchetype& {
  struct CachedArchetype {
    BowBodyKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  BowResolvedGeometry const geometry = resolve_bow_geometry(config);
  BowBodyKey const key{
      quantize_bow_value(geometry.string_radius),
      quantize_bow_value(geometry.depth),
      quantize_bow_value(config.bow_x),
      quantize_bow_value(config.bow_forward_offset),
      quantize_bow_value(geometry.half_height),
      0,
      quantize_bow_value(k_bow_length_scale),
      quantize_bow_value(geometry.curve_factor),
      config.material_id,
  };
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  BowPlane const plane = resolve_bow_plane(config, geometry);
  QVector3D const top_end = plane.top_end;
  QVector3D const bot_end = plane.bot_end;
  QVector3D const nock_rest = plane.string_center;

  RenderArchetypeBuilder builder{
      "bow_string_" + std::to_string(key.rod_radius_key) + "_" +
      std::to_string(key.bow_depth_key) + "_" + std::to_string(key.bow_x_key) + "_" +
      std::to_string(key.bow_forward_offset_key) + "_" +
      std::to_string(key.bow_top_y_key) + "_" + std::to_string(key.bow_bot_y_key) +
      "_" + std::to_string(key.bow_height_scale_key) + "_" +
      std::to_string(key.bow_curve_factor_key) + "_" + std::to_string(key.material_id)};

  add_generated_equipment_primitive(builder,
                                    generated_cylinder(top_end,
                                                       nock_rest,
                                                       geometry.string_radius,
                                                       k_bow_string_slot,
                                                       1.0F,
                                                       config.material_id));
  add_generated_equipment_primitive(builder,
                                    generated_cylinder(nock_rest,
                                                       bot_end,
                                                       geometry.string_radius,
                                                       k_bow_string_slot,
                                                       1.0F,
                                                       config.material_id));

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto nocked_arrow_archetype(const BowRenderConfig& config) -> const RenderArchetype& {
  struct CachedArchetype {
    BowBodyKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  BowResolvedGeometry const geometry = resolve_bow_geometry(config);
  BowBodyKey const key{
      quantize_bow_value(k_nocked_arrow_length),
      quantize_bow_value(geometry.string_setback),
      quantize_bow_value(config.bow_x),
      quantize_bow_value(config.bow_forward_offset),
      0,
      0,
      0,
      0,
      config.material_id,
  };
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  BowPlane const plane = resolve_bow_plane(config, geometry);
  QVector3D const nock =
      plane.string_center + QVector3D(0.0F, k_nocked_arrow_rest_height, 0.0F);
  QVector3D const along(0.0F, 0.0F, 1.0F);
  QVector3D const tip = nock + along * k_nocked_arrow_length;
  QVector3D const head_base = tip - along * k_nocked_arrow_head_length;
  QVector3D const fletching_end = nock + along * k_nocked_arrow_fletching_length;

  RenderArchetypeBuilder builder{
      "bow_nocked_arrow_" + std::to_string(key.rod_radius_key) + "_" +
      std::to_string(key.bow_depth_key) + "_" + std::to_string(key.material_id)};
  add_generated_equipment_primitive(builder,
                                    generated_cylinder(nock,
                                                       head_base,
                                                       k_nocked_arrow_shaft_radius,
                                                       k_arrow_shaft_slot,
                                                       1.0F,
                                                       config.material_id));
  add_generated_equipment_primitive(builder,
                                    generated_cone(head_base,
                                                   tip,
                                                   k_nocked_arrow_head_radius,
                                                   k_arrow_head_slot,
                                                   1.0F,
                                                   config.material_id));
  add_generated_equipment_primitive(builder,
                                    generated_cone(fletching_end,
                                                   nock,
                                                   k_nocked_arrow_fletching_radius,
                                                   k_arrow_fletching_slot,
                                                   1.0F,
                                                   config.material_id));

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto bow_body_transform(const QMatrix4x4& parent,
                        const QVector3D& grip,
                        const QVector3D& outward,
                        const QVector3D& bow_up,
                        const QVector3D& bow_forward) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(outward, 0.0F));
  local.setColumn(1, QVector4D(bow_up, 0.0F));
  local.setColumn(2, QVector4D(bow_forward, 0.0F));
  local.setColumn(3, QVector4D(grip, 1.0F));
  return parent * local;
}
} // namespace

BowRenderer::BowRenderer(BowRenderConfig config)
    : m_base(config) {
}

void BowRenderer::render(const DrawContext& ctx,
                         const BodyFrames& frames,
                         const HumanoidPalette& palette,
                         const HumanoidAnimationContext& anim,
                         EquipmentBatch& batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void BowRenderer::submit(const BowRenderConfig& m_config,
                         const DrawContext& ctx,
                         const BodyFrames& frames,
                         const HumanoidPalette& palette,
                         const HumanoidAnimationContext& anim,
                         EquipmentBatch& batch) {
  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D forward(0.0F, 0.0F, 1.0F);

  AttachmentFrame const grip_socket = Render::Humanoid::socket_attachment_frame(
      frames.hand_r, Render::Humanoid::HumanoidSocket::GripR);
  QVector3D const grip = grip_socket.origin;
  BowResolvedGeometry const geometry = resolve_bow_geometry(m_config);

  QVector3D outward = grip_socket.right;
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  }
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  } else {
    outward.normalize();
  }

  bool const is_bow_attacking = anim.inputs.is_attacking && !anim.inputs.is_melee;
  float const hold_blend = (is_bow_attacking && !anim.inputs.is_in_hold_mode)
                               ? 0.0F
                               : hold_transition_amount(anim.inputs);
  QVector3D bow_up = up;
  if (hold_blend > 1e-4F) {
    QVector3D const hold_axis(0.0F, 0.90F, 0.44F);
    bow_up = up * (1.0F - hold_blend) + hold_axis * hold_blend;
    bow_up.normalize();
  }

  QVector3D bow_forward = QVector3D::crossProduct(outward, bow_up);
  if (bow_forward.lengthSquared() < 1e-6F) {
    bow_forward = forward;
  } else {
    bow_forward.normalize();
  }

  QVector3D const bow_base = grip +
                             outward * (m_config.bow_x + k_bow_grip_side_offset) +
                             bow_forward * m_config.bow_forward_offset;
  std::array<QVector3D, 1> const body_palette{k_dark_bow_color};

  if (m_config.draw_body) {
    append_equipment_archetype(
        batch,
        bow_body_archetype(m_config),
        bow_body_transform(ctx.model, grip, outward, bow_up, bow_forward),
        body_palette);
  }

  QVector3D const string_plane_center =
      bow_base - bow_forward * geometry.string_setback;
  QVector3D const top_end = string_plane_center + bow_up * geometry.half_height;
  QVector3D const bot_end = string_plane_center - bow_up * geometry.half_height;

  QVector3D nock = string_plane_center;
  auto const nock_from_string_hand = [&](const QVector3D& string_hand) {
    float const nock_along =
        clamp_f(QVector3D::dotProduct(string_hand - string_plane_center, bow_up),
                -geometry.half_height + 0.05F,
                geometry.half_height - 0.05F);
    float const nock_depth =
        clamp_f(QVector3D::dotProduct(string_hand - string_plane_center, bow_forward),
                -geometry.max_draw_depth,
                geometry.max_draw_depth);
    return string_plane_center + bow_up * nock_along + bow_forward * nock_depth;
  };
  if (hold_blend > 1e-4F) {
    QVector3D const rest_nock = string_plane_center + bow_up * 0.02F;
    QVector3D const string_hand_nock = nock_from_string_hand(frames.hand_l.origin);
    nock = rest_nock * (1.0F - hold_blend) + string_hand_nock * hold_blend;
  } else if (is_bow_attacking) {
    nock = nock_from_string_hand(frames.hand_l.origin);
  } else {
    nock = string_plane_center;
  }
  std::array<QVector3D, 1> const string_palette{m_config.string_color};

  if (m_config.draw_string) {
    append_equipment_archetype(
        batch,
        single_cylinder_archetype(
            geometry.string_radius, m_config.material_id, "bow_string"),
        oriented_segment_transform(ctx.model, top_end, nock - top_end, outward),
        string_palette);
    append_equipment_archetype(
        batch,
        single_cylinder_archetype(
            geometry.string_radius, m_config.material_id, "bow_string"),
        oriented_segment_transform(ctx.model, nock, bot_end - nock, outward),
        string_palette);
  }

  if (is_bow_attacking) {
    std::array<QVector3D, 1> const attack_string_palette{m_config.string_color * 0.9F};
    append_equipment_archetype(
        batch,
        single_cylinder_archetype(
            geometry.attack_string_radius, m_config.material_id, "bow_attack_string"),
        oriented_segment_transform(
            ctx.model, frames.hand_l.origin, nock - frames.hand_l.origin, outward),
        attack_string_palette);
  }

  float attack_phase = 0.0F;
  if (is_bow_attacking) {
    attack_phase = std::fmod(anim.inputs.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  constexpr float k_attack_arrow_window_end = 0.52F;
  bool const attack_window_active =
      is_bow_attacking &&
      (attack_phase >= 0.0F && attack_phase < k_attack_arrow_window_end);

  auto arrow_visible = [&m_config, is_bow_attacking, attack_window_active]() -> bool {
    switch (m_config.arrow_visibility) {
    case ArrowVisibility::Hidden:
      return false;
    case ArrowVisibility::AttackCycleOnly:
      return attack_window_active;
    case ArrowVisibility::IdleAndAttackCycle:
      if (!is_bow_attacking) {
        return true;
      }
      return attack_window_active;
    }
    return attack_window_active;
  };

  bool const show_arrow = arrow_visible();

  if (show_arrow) {
    QVector3D const tail = nock - bow_forward * 0.06F;
    QVector3D const tip = tail + bow_forward * 0.90F;
    std::array<QVector3D, 3> const arrow_palette{
        palette.wood, m_config.metal_color, m_config.fletching_color};
    append_equipment_archetype(
        batch,
        arrow_shaft_archetype(0.018F, m_config.material_id, "bow_arrow_shaft"),
        oriented_segment_transform(ctx.model, tail, tip - tail, outward),
        arrow_palette);

    QVector3D const head_base = tip - bow_forward * 0.10F;
    append_equipment_archetype(
        batch,
        arrow_head_archetype(0.05F, m_config.material_id, "bow_arrow_head"),
        oriented_segment_transform(ctx.model, head_base, tip - head_base, outward),
        arrow_palette);

    QVector3D const f1b = tail - bow_forward * 0.02F;
    QVector3D const f1a = f1b - bow_forward * 0.06F;
    QVector3D const f2b = tail + bow_forward * 0.02F;
    QVector3D const f2a = f2b + bow_forward * 0.06F;

    append_equipment_archetype(
        batch,
        arrow_fletching_archetype(0.04F, m_config.material_id, "bow_arrow_fletching"),
        oriented_segment_transform(ctx.model, f1b, f1a - f1b, outward),
        arrow_palette);
    append_equipment_archetype(
        batch,
        arrow_fletching_archetype(0.04F, m_config.material_id, "bow_arrow_fletching"),
        oriented_segment_transform(ctx.model, f2a, f2b - f2a, outward),
        arrow_palette);
  }
}

auto bow_fill_role_colors(const HumanoidPalette& palette,
                          QVector3D* out,
                          std::size_t max) -> std::uint32_t {
  if (max < k_bow_role_count) {
    return 0;
  }
  constexpr BowRenderConfig cfg{};
  out[k_bow_body_slot] = k_dark_bow_color;
  out[k_bow_string_slot] = cfg.string_color;
  out[k_bow_arrow_shaft_role] = palette.wood;
  out[k_bow_arrow_head_role] = cfg.metal_color;
  out[k_bow_arrow_fletching_role] = cfg.fletching_color;
  return k_bow_role_count;
}

auto bow_make_static_attachments(const BowRenderConfig& config,
                                 std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 3> {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  auto const bind_grip = Render::Humanoid::socket_attachment_frame(
      Render::Humanoid::humanoid_bind_body_frames().hand_r,
      Render::Humanoid::HumanoidSocket::GripR);
  QMatrix4x4 bind_socket;
  bind_socket.setColumn(0, QVector4D(bind_grip.right, 0.0F));
  bind_socket.setColumn(1, QVector4D(bind_grip.up, 0.0F));
  bind_socket.setColumn(2, QVector4D(bind_grip.forward, 0.0F));
  bind_socket.setColumn(3, QVector4D(bind_grip.origin, 1.0F));
  bool bind_socket_invertible = false;
  QMatrix4x4 const bind_socket_inverse = bind_socket.inverted(&bind_socket_invertible);
  QVector3D const grip = bind_socket.column(3).toVector3D();
  QVector3D outward = bind_socket.column(0).toVector3D();
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  } else {
    outward.normalize();
  }
  QMatrix4x4 const unit_pose = bow_body_transform(QMatrix4x4{},
                                                  grip,
                                                  outward,
                                                  QVector3D(0.0F, 1.0F, 0.0F),
                                                  QVector3D(0.0F, 0.0F, 1.0F));
  QMatrix4x4 const mesh_from_socket =
      bind_socket_invertible ? bind_socket_inverse * unit_pose : unit_pose;

  auto body_spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &bow_body_archetype(config),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = mesh_from_socket,
  });
  body_spec.palette_role_remap[k_bow_body_slot] = base_role_byte;

  auto string_spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &bow_string_archetype(config),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = mesh_from_socket,
  });
  string_spec.palette_role_remap[k_bow_string_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  auto arrow_spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &nocked_arrow_archetype(config),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = mesh_from_socket,
  });
  arrow_spec.palette_role_remap[k_arrow_shaft_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_bow_arrow_shaft_role);
  arrow_spec.palette_role_remap[k_arrow_head_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_bow_arrow_head_role);
  arrow_spec.palette_role_remap[k_arrow_fletching_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_bow_arrow_fletching_role);

  return {body_spec, string_spec, arrow_spec};
}

} // namespace Render::GL
