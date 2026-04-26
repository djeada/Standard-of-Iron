#include "bow_renderer.h"
#include "../../entity/registry.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../static_attachment_spec.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "../generated_equipment.h"
#include "../oriented_archetype_utils.h"
#include "../primitive_archetype_utils.h"
#include "arrow_archetype_utils.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace Render::GL {

using Render::Geom::clamp_f;

namespace {
constexpr QVector3D k_dark_bow_color(0.05F, 0.035F, 0.02F);

enum BowPaletteSlot : std::uint8_t {
  k_bow_body_slot = 0U,
  k_bow_string_slot = 1U,
};

struct BowBodyKey {
  int rod_radius_key{0};
  int bow_depth_key{0};
  int bow_x_key{0};
  int bow_top_y_key{0};
  int bow_bot_y_key{0};
  int bow_height_scale_key{0};
  int bow_curve_factor_key{0};
  int material_id{0};
};

auto operator==(const BowBodyKey &lhs, const BowBodyKey &rhs) -> bool {
  return lhs.rod_radius_key == rhs.rod_radius_key &&
         lhs.bow_depth_key == rhs.bow_depth_key &&
         lhs.bow_x_key == rhs.bow_x_key &&
         lhs.bow_top_y_key == rhs.bow_top_y_key &&
         lhs.bow_bot_y_key == rhs.bow_bot_y_key &&
         lhs.bow_height_scale_key == rhs.bow_height_scale_key &&
         lhs.bow_curve_factor_key == rhs.bow_curve_factor_key &&
         lhs.material_id == rhs.material_id;
}

auto quantize_bow_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

auto bow_body_archetype(const BowRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    BowBodyKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  BowBodyKey const key{
      quantize_bow_value(config.bow_rod_radius),
      quantize_bow_value(config.bow_depth),
      quantize_bow_value(config.bow_x),
      quantize_bow_value(config.bow_top_y),
      quantize_bow_value(config.bow_bot_y),
      quantize_bow_value(config.bow_height_scale),
      quantize_bow_value(config.bow_curve_factor),
      config.material_id,
  };
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const bow_half_height =
      (config.bow_top_y - config.bow_bot_y) * 0.5F * config.bow_height_scale;
  float const bow_plane_x = config.bow_x + 0.02F;
  QVector3D const top_end(bow_plane_x, bow_half_height, 0.0F);
  QVector3D const bot_end(bow_plane_x, -bow_half_height, 0.0F);
  QVector3D const ctrl(bow_plane_x, 0.45F * config.bow_curve_factor,
                       config.bow_depth * 0.6F * config.bow_curve_factor);

  auto q_bezier = [](const QVector3D &a, const QVector3D &c, const QVector3D &b,
                     float t) {
    float const u = 1.0F - t;
    return u * u * a + 2.0F * u * t * c + t * t * b;
  };

  constexpr int k_bow_segments = 22;
  RenderArchetypeBuilder builder{
      "bow_body_" + std::to_string(key.rod_radius_key) + "_" +
      std::to_string(key.bow_depth_key) + "_" + std::to_string(key.bow_x_key) +
      "_" + std::to_string(key.bow_top_y_key) + "_" +
      std::to_string(key.bow_bot_y_key) + "_" +
      std::to_string(key.bow_height_scale_key) + "_" +
      std::to_string(key.bow_curve_factor_key) + "_" +
      std::to_string(key.material_id)};

  QVector3D prev = bot_end;
  for (int i = 1; i <= k_bow_segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_bow_segments);
    QVector3D const cur = q_bezier(bot_end, ctrl, top_end, t);
    add_generated_equipment_primitive(
        builder, generated_cylinder(prev, cur, config.bow_rod_radius,
                                    k_bow_body_slot, 1.0F, config.material_id));
    prev = cur;
  }

  add_generated_equipment_primitive(
      builder, generated_cylinder(QVector3D(0.0F, -0.05F, 0.0F),
                                  QVector3D(0.0F, 0.05F, 0.0F),
                                  config.bow_rod_radius * 1.45F,
                                  k_bow_body_slot, 1.0F, config.material_id));

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto bow_string_archetype(const BowRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    BowBodyKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  BowBodyKey const key{
      quantize_bow_value(config.string_radius),
      quantize_bow_value(config.bow_depth),
      quantize_bow_value(config.bow_x),
      quantize_bow_value(config.bow_top_y),
      quantize_bow_value(config.bow_bot_y),
      quantize_bow_value(config.bow_height_scale),
      quantize_bow_value(config.bow_curve_factor),
      config.material_id,
  };
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const bow_half_height =
      (config.bow_top_y - config.bow_bot_y) * 0.5F * config.bow_height_scale;
  float const bow_plane_x = config.bow_x + 0.02F;
  QVector3D const top_end(bow_plane_x, bow_half_height, 0.0F);
  QVector3D const bot_end(bow_plane_x, -bow_half_height, 0.0F);
  QVector3D const nock_rest(bow_plane_x, 0.0F, 0.0F);

  RenderArchetypeBuilder builder{
      "bow_string_" + std::to_string(key.rod_radius_key) + "_" +
      std::to_string(key.bow_depth_key) + "_" + std::to_string(key.bow_x_key) +
      "_" + std::to_string(key.bow_top_y_key) + "_" +
      std::to_string(key.bow_bot_y_key) + "_" +
      std::to_string(key.bow_height_scale_key) + "_" +
      std::to_string(key.bow_curve_factor_key) + "_" +
      std::to_string(key.material_id)};

  add_generated_equipment_primitive(
      builder, generated_cylinder(top_end, nock_rest, config.string_radius,
                                  k_bow_string_slot, 1.0F, config.material_id));
  add_generated_equipment_primitive(
      builder, generated_cylinder(nock_rest, bot_end, config.string_radius,
                                  k_bow_string_slot, 1.0F, config.material_id));

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto bow_body_transform(const QMatrix4x4 &parent, const QVector3D &grip,
                        const QVector3D &outward) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(outward, 0.0F));
  local.setColumn(1, QVector4D(0.0F, 1.0F, 0.0F, 0.0F));
  local.setColumn(2, QVector4D(0.0F, 0.0F, 1.0F, 0.0F));
  local.setColumn(3, QVector4D(grip, 1.0F));
  return parent * local;
}
} // namespace

BowRenderer::BowRenderer(BowRenderConfig config) : m_base(std::move(config)) {}

void BowRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                         const HumanoidPalette &palette,
                         const HumanoidAnimationContext &anim,
                         EquipmentBatch &batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void BowRenderer::submit(const BowRenderConfig &m_config,
                         const DrawContext &ctx, const BodyFrames &frames,
                         const HumanoidPalette &palette,
                         const HumanoidAnimationContext &anim,
                         EquipmentBatch &batch) {
  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D forward(0.0F, 0.0F, 1.0F);

  QVector3D const grip = frames.hand_r.origin;

  float const bow_half_height = (m_config.bow_top_y - m_config.bow_bot_y) *
                                0.5F * m_config.bow_height_scale;
  float const bow_mid_y = grip.y();
  float const bow_top_y = bow_mid_y + bow_half_height;
  float const bow_bot_y = bow_mid_y - bow_half_height;

  QVector3D outward = frames.hand_r.right;
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  }
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  } else {
    outward.normalize();
  }

  QVector3D const side = outward * 0.02F;
  std::array<QVector3D, 1> const body_palette{k_dark_bow_color};

  append_equipment_archetype(batch, bow_body_archetype(m_config),
                             bow_body_transform(ctx.model, grip, outward),
                             body_palette);

  float const bow_plane_x = grip.x() + m_config.bow_x + side.x();
  float const bow_plane_z = grip.z() + side.z();

  QVector3D const top_end(bow_plane_x, bow_top_y, bow_plane_z);
  QVector3D const bot_end(bow_plane_x, bow_bot_y, bow_plane_z);

  QVector3D const string_hand = frames.hand_l.origin;
  QVector3D const nock(
      bow_plane_x,
      clamp_f(string_hand.y(), bow_bot_y + 0.05F, bow_top_y - 0.05F),
      clamp_f(string_hand.z(), bow_plane_z - 0.30F, bow_plane_z + 0.30F));
  std::array<QVector3D, 1> const string_palette{m_config.string_color};

  append_equipment_archetype(
      batch,
      single_cylinder_archetype(m_config.string_radius, m_config.material_id,
                                "bow_string"),
      oriented_segment_transform(ctx.model, top_end, nock - top_end, outward),
      string_palette);
  append_equipment_archetype(
      batch,
      single_cylinder_archetype(m_config.string_radius, m_config.material_id,
                                "bow_string"),
      oriented_segment_transform(ctx.model, nock, bot_end - nock, outward),
      string_palette);

  bool const is_bow_attacking =
      anim.inputs.is_attacking && !anim.inputs.is_melee;
  if (is_bow_attacking) {
    std::array<QVector3D, 1> const attack_string_palette{m_config.string_color *
                                                         0.9F};
    append_equipment_archetype(
        batch,
        single_cylinder_archetype(0.0045F, m_config.material_id,
                                  "bow_attack_string"),
        oriented_segment_transform(ctx.model, frames.hand_l.origin,
                                   nock - frames.hand_l.origin, outward),
        attack_string_palette);
  }

  float attack_phase = 0.0F;
  if (is_bow_attacking) {
    attack_phase =
        std::fmod(anim.inputs.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
  }

  constexpr float k_attack_arrow_window_end = 0.52F;
  bool const attack_window_active =
      is_bow_attacking &&
      (attack_phase >= 0.0F && attack_phase < k_attack_arrow_window_end);

  auto arrow_visible = [&m_config, is_bow_attacking,
                        attack_window_active]() -> bool {
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
    QVector3D const tail = nock - forward * 0.06F;
    QVector3D const tip = tail + forward * 0.90F;
    std::array<QVector3D, 3> const arrow_palette{
        palette.wood, m_config.metal_color, m_config.fletching_color};
    append_equipment_archetype(
        batch,
        arrow_shaft_archetype(0.018F, m_config.material_id, "bow_arrow_shaft"),
        oriented_segment_transform(ctx.model, tail, tip - tail, outward),
        arrow_palette);

    QVector3D const head_base = tip - forward * 0.10F;
    append_equipment_archetype(
        batch,
        arrow_head_archetype(0.05F, m_config.material_id, "bow_arrow_head"),
        oriented_segment_transform(ctx.model, head_base, tip - head_base,
                                   outward),
        arrow_palette);

    QVector3D const f1b = tail - forward * 0.02F;
    QVector3D const f1a = f1b - forward * 0.06F;
    QVector3D const f2b = tail + forward * 0.02F;
    QVector3D const f2a = f2b + forward * 0.06F;

    append_equipment_archetype(
        batch,
        arrow_fletching_archetype(0.04F, m_config.material_id,
                                  "bow_arrow_fletching"),
        oriented_segment_transform(ctx.model, f1b, f1a - f1b, outward),
        arrow_palette);
    append_equipment_archetype(
        batch,
        arrow_fletching_archetype(0.04F, m_config.material_id,
                                  "bow_arrow_fletching"),
        oriented_segment_transform(ctx.model, f2a, f2b - f2a, outward),
        arrow_palette);
  }
}

auto bow_fill_role_colors(const HumanoidPalette &, QVector3D *out,
                          std::size_t max) -> std::uint32_t {
  if (max < kBowRoleCount) {
    return 0;
  }
  constexpr BowRenderConfig cfg{};
  out[k_bow_body_slot] = k_dark_bow_color;
  out[k_bow_string_slot] = cfg.string_color;
  return kBowRoleCount;
}

auto bow_make_static_attachments(const BowRenderConfig &config,
                                 std::uint16_t socket_bone_index,
                                 std::uint8_t base_role_byte,
                                 const QMatrix4x4 &bind_hand_r_matrix,
                                 const QVector3D &bind_hand_r_right_world)
    -> std::array<Render::Creature::StaticAttachmentSpec, 2> {
  QVector3D const grip = bind_hand_r_matrix.column(3).toVector3D();
  QVector3D outward = bind_hand_r_right_world;
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-6F) {
    outward = QVector3D(-1.0F, 0.0F, 0.0F);
  } else {
    outward.normalize();
  }
  QMatrix4x4 const bind_pose = bow_body_transform(QMatrix4x4{}, grip, outward);

  auto body_spec = Render::Equipment::build_static_attachment({
      .archetype = &bow_body_archetype(config),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_pose,
  });
  body_spec.palette_role_remap[k_bow_body_slot] = base_role_byte;

  auto string_spec = Render::Equipment::build_static_attachment({
      .archetype = &bow_string_archetype(config),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_pose,
  });
  string_spec.palette_role_remap[k_bow_string_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  return {body_spec, string_spec};
}

} // namespace Render::GL
