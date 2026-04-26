#include "roman_scutum.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/style_palette.h"
#include "../../render_archetype.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

namespace {

constexpr std::uint8_t k_red_even = 0;
constexpr std::uint8_t k_red_odd = 1;
constexpr std::uint8_t k_bronze_ridge = 2;
constexpr std::uint8_t k_bronze_ring = 3;
constexpr std::uint8_t k_bronze_boss = 4;
constexpr std::uint8_t k_bronze_rim = 5;
constexpr std::uint8_t k_bronze_rivet = 6;

auto roman_scutum_archetype() -> const RenderArchetype & {
  static const RenderArchetype kArchetype = []() {
    constexpr float shield_height = 1.2F;
    constexpr float shield_width = 0.65F;
    constexpr float shield_curve = 0.25F;
    constexpr float rim_thickness = 0.015F;
    constexpr float boss_radius = 0.12F;

    constexpr QVector3D shield_center{0.0F, 0.0F, 0.15F};
    constexpr QVector3D shield_forward{0.0F, 0.0F, 1.0F};
    constexpr QVector3D shield_up{0.0F, 1.0F, 0.0F};
    constexpr QVector3D shield_right{1.0F, 0.0F, 0.0F};

    RenderArchetypeBuilder builder{"roman_scutum"};

    constexpr int vertical_segments = 12;
    constexpr int horizontal_segments = 16;
    for (int v = 0; v < vertical_segments; ++v) {
      for (int h = 0; h < horizontal_segments; ++h) {
        float const v_t =
            static_cast<float>(v) / static_cast<float>(vertical_segments);
        float const h_t =
            static_cast<float>(h) / static_cast<float>(horizontal_segments);
        float const y_local = (v_t - 0.5F) * shield_height;
        float const x_local = (h_t - 0.5F) * shield_width;
        float const curve_offset =
            shield_curve * (1.0F - std::abs(x_local / (shield_width * 0.5F)));
        QVector3D const pos = shield_center + shield_up * y_local +
                              shield_right * x_local +
                              shield_forward * curve_offset;
        QMatrix4x4 m;
        m.translate(pos);
        m.scale(0.03F, 0.05F, 0.01F);
        const std::uint8_t slot = (v % 2 == 0) ? k_red_even : k_red_odd;
        builder.add_palette_mesh(get_unit_sphere(), m, slot);
      }
    }

    constexpr int ridge_segments = 10;
    for (int i = 0; i < ridge_segments; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(ridge_segments - 1);
      float const y_local = (t - 0.5F) * shield_height * 0.9F;
      QVector3D const pos = shield_center + shield_up * y_local +
                            shield_forward * (shield_curve + 0.02F);
      QMatrix4x4 m;
      m.translate(pos);
      m.scale(0.025F, 0.06F, 0.015F);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_ridge);
    }

    QVector3D const boss_center =
        shield_center + shield_forward * (shield_curve + 0.08F);

    for (int i = 0; i < 12; ++i) {
      float const angle =
          (static_cast<float>(i) / 12.0F) * 2.0F * std::numbers::pi_v<float>;
      QVector3D const ring_pos =
          boss_center + shield_right * (boss_radius * std::cos(angle)) +
          shield_up * (boss_radius * std::sin(angle));
      QMatrix4x4 m;
      m.translate(ring_pos);
      m.scale(0.018F);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_ring);
    }

    QMatrix4x4 boss_m;
    boss_m.translate(boss_center);
    boss_m.scale(boss_radius * 0.8F);
    builder.add_palette_mesh(get_unit_sphere(), boss_m, k_bronze_boss);

    float const y_pos_top = shield_height * 0.48F;
    for (int i = 0; i < 10; ++i) {
      float const t = static_cast<float>(i) / 9.0F;
      float const x_local = (t - 0.5F) * shield_width * 0.95F;
      float const curve_off =
          shield_curve * (1.0F - std::abs(x_local / (shield_width * 0.5F)));
      QVector3D const pos = shield_center + shield_up * y_pos_top +
                            shield_right * x_local + shield_forward * curve_off;
      QMatrix4x4 m;
      m.translate(pos);
      m.scale(rim_thickness);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_rim);
    }

    float const y_pos_bot = -shield_height * 0.48F;
    for (int i = 0; i < 10; ++i) {
      float const t = static_cast<float>(i) / 9.0F;
      float const x_local = (t - 0.5F) * shield_width * 0.95F;
      float const curve_off =
          shield_curve * (1.0F - std::abs(x_local / (shield_width * 0.5F)));
      QVector3D const pos = shield_center + shield_up * y_pos_bot +
                            shield_right * x_local + shield_forward * curve_off;
      QMatrix4x4 m;
      m.translate(pos);
      m.scale(rim_thickness);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_rim);
    }

    for (int side = 0; side < 2; ++side) {
      float const x_pos_side =
          (side == 0 ? -1.0F : 1.0F) * shield_width * 0.48F;
      float const curve_off =
          shield_curve * (1.0F - std::abs(x_pos_side / (shield_width * 0.5F)));
      for (int i = 0; i < 12; ++i) {
        float const t = static_cast<float>(i) / 11.0F;
        float const y_local = (t - 0.5F) * shield_height * 0.95F;
        QVector3D const pos = shield_center + shield_up * y_local +
                              shield_right * x_pos_side +
                              shield_forward * curve_off;
        QMatrix4x4 m;
        m.translate(pos);
        m.scale(rim_thickness);
        builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_rim);
      }
    }

    for (int i = 0; i < 8; ++i) {
      float const angle =
          (static_cast<float>(i) / 8.0F) * 2.0F * std::numbers::pi_v<float>;
      float const rivet_dist = boss_radius * 1.3F;
      QVector3D const rivet_pos =
          boss_center + shield_right * (rivet_dist * std::cos(angle)) +
          shield_up * (rivet_dist * std::sin(angle));
      QMatrix4x4 m;
      m.translate(rivet_pos);
      m.scale(0.012F);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_rivet);
    }

    return std::move(builder).build();
  }();
  return kArchetype;
}

auto hand_l_basis_transform(const QMatrix4x4 &parent,
                            const AttachmentFrame &hand_l) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(hand_l.right, 0.0F));
  local.setColumn(1, QVector4D(hand_l.up, 0.0F));
  local.setColumn(2, QVector4D(hand_l.forward, 0.0F));
  local.setColumn(3, QVector4D(hand_l.origin, 1.0F));
  return parent * local;
}

} // namespace

void RomanScutumRenderer::render(const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &anim,
                                 EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanScutumRenderer::submit(const RomanScutumConfig &,
                                 const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &anim,
                                 EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &hand_l = frames.hand_l;
  if (hand_l.radius <= 0.0F) {
    return;
  }

  QVector3D const shield_red =
      saturate_color(palette.cloth * QVector3D(1.5F, 0.3F, 0.3F));
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.0F, 0.5F));

  std::array<QVector3D, kRomanScutumRoleCount> const palette_slots{
      shield_red * 0.975F,  shield_red * 1.025F, bronze_color * 0.9F,
      bronze_color,         bronze_color * 1.1F, bronze_color * 0.95F,
      bronze_color * 1.15F,
  };

  append_equipment_archetype(batch, roman_scutum_archetype(),
                             hand_l_basis_transform(ctx.model, hand_l),
                             palette_slots);
}

auto roman_scutum_fill_role_colors(const HumanoidPalette &palette,
                                   QVector3D *out,
                                   std::size_t max) -> std::uint32_t {
  if (max < kRomanScutumRoleCount) {
    return 0;
  }
  QVector3D const shield_red =
      saturate_color(palette.cloth * QVector3D(1.5F, 0.3F, 0.3F));
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.0F, 0.5F));
  out[k_red_even] = shield_red * 0.975F;
  out[k_red_odd] = shield_red * 1.025F;
  out[k_bronze_ridge] = bronze_color * 0.9F;
  out[k_bronze_ring] = bronze_color;
  out[k_bronze_boss] = bronze_color * 1.1F;
  out[k_bronze_rim] = bronze_color * 0.95F;
  out[k_bronze_rivet] = bronze_color * 1.15F;
  return kRomanScutumRoleCount;
}

auto roman_scutum_make_static_attachment(std::uint16_t socket_bone_index,
                                         std::uint8_t base_role_byte,
                                         const QMatrix4x4 &bind_hand_l_matrix)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_scutum_archetype(),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_hand_l_matrix,
  });
  spec.palette_role_remap[k_red_even] = base_role_byte;
  spec.palette_role_remap[k_red_odd] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_bronze_ridge] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_bronze_ring] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  spec.palette_role_remap[k_bronze_boss] =
      static_cast<std::uint8_t>(base_role_byte + 4U);
  spec.palette_role_remap[k_bronze_rim] =
      static_cast<std::uint8_t>(base_role_byte + 5U);
  spec.palette_role_remap[k_bronze_rivet] =
      static_cast<std::uint8_t>(base_role_byte + 6U);
  return spec;
}

} // namespace Render::GL
