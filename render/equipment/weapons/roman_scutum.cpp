#include "roman_scutum.h"

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/style_palette.h"
#include "../equipment_submit.h"

#include "../../render_archetype.h"

#include <array>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanScutumPaletteSlot : std::uint8_t {
  k_shield_slot = 0U,
  k_bronze_slot = 1U,
};

auto roman_scutum_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder{"roman_scutum"};

    constexpr float shield_height = 1.05F;
    constexpr float shield_width = 0.56F;
    constexpr float shield_curve = 0.22F;
    constexpr float rim_thickness = 0.015F;
    constexpr float boss_radius = 0.10F;
    QVector3D const shield_center_local{0.0F, 0.02F, 0.12F};

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

        QMatrix4x4 m;
        m.translate(shield_center_local +
                    QVector3D(x_local, y_local, curve_offset));
        m.scale(0.03F, 0.05F, 0.01F);
        builder.add_palette_mesh(get_unit_sphere(), m, k_shield_slot, nullptr,
                                 1.0F, 4);
      }
    }

    constexpr int ridge_segments = 10;
    for (int i = 0; i < ridge_segments; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(ridge_segments - 1);
      float const y_local = (t - 0.5F) * shield_height * 0.9F;

      QMatrix4x4 m;
      m.translate(shield_center_local +
                  QVector3D(0.0F, y_local, shield_curve + 0.02F));
      m.scale(0.025F, 0.06F, 0.015F);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_slot, nullptr,
                               1.0F, 4);
    }

    QVector3D const boss_center =
        shield_center_local + QVector3D(0.0F, 0.0F, shield_curve + 0.08F);

    for (int i = 0; i < 12; ++i) {
      float const angle =
          (static_cast<float>(i) / 12.0F) * 2.0F * std::numbers::pi_v<float>;
      QMatrix4x4 m;
      m.translate(boss_center + QVector3D(boss_radius * std::cos(angle),
                                          boss_radius * std::sin(angle), 0.0F));
      m.scale(0.018F);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_slot, nullptr,
                               1.0F, 4);
    }

    QMatrix4x4 boss;
    boss.translate(boss_center);
    boss.scale(boss_radius * 0.8F);
    builder.add_palette_mesh(get_unit_sphere(), boss, k_bronze_slot, nullptr,
                             1.0F, 4);

    float const y_pos_top = shield_height * 0.48F;
    float const y_pos_bottom = -shield_height * 0.48F;
    for (int i = 0; i < 10; ++i) {
      float const t = static_cast<float>(i) / 9.0F;
      float const x_local = (t - 0.5F) * shield_width * 0.95F;
      float const curve_off =
          shield_curve * (1.0F - std::abs(x_local / (shield_width * 0.5F)));

      QMatrix4x4 top;
      top.translate(shield_center_local +
                    QVector3D(x_local, y_pos_top, curve_off));
      top.scale(rim_thickness);
      builder.add_palette_mesh(get_unit_sphere(), top, k_bronze_slot, nullptr,
                               1.0F, 4);

      QMatrix4x4 bottom;
      bottom.translate(shield_center_local +
                       QVector3D(x_local, y_pos_bottom, curve_off));
      bottom.scale(rim_thickness);
      builder.add_palette_mesh(get_unit_sphere(), bottom, k_bronze_slot,
                               nullptr, 1.0F, 4);
    }

    for (int side = 0; side < 2; ++side) {
      float const x_pos_side =
          (side == 0 ? -1.0F : 1.0F) * shield_width * 0.48F;
      float const curve_off =
          shield_curve * (1.0F - std::abs(x_pos_side / (shield_width * 0.5F)));

      for (int i = 0; i < 12; ++i) {
        float const t = static_cast<float>(i) / 11.0F;
        float const y_local = (t - 0.5F) * shield_height * 0.95F;

        QMatrix4x4 m;
        m.translate(shield_center_local +
                    QVector3D(x_pos_side, y_local, curve_off));
        m.scale(rim_thickness);
        builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_slot, nullptr,
                                 1.0F, 4);
      }
    }

    for (int i = 0; i < 8; ++i) {
      float const angle =
          (static_cast<float>(i) / 8.0F) * 2.0F * std::numbers::pi_v<float>;
      float const rivet_dist = boss_radius * 1.3F;

      QMatrix4x4 m;
      m.translate(boss_center + QVector3D(rivet_dist * std::cos(angle),
                                          rivet_dist * std::sin(angle), 0.0F));
      m.scale(0.012F);
      builder.add_palette_mesh(get_unit_sphere(), m, k_bronze_slot, nullptr,
                               1.0F, 4);
    }

    return std::move(builder).build();
  }();
  return archetype;
}

auto roman_scutum_transform(const QMatrix4x4 &parent,
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

  using HP = HumanProportions;

  QVector3D const shield_red =
      saturate_color(palette.cloth * QVector3D(1.5F, 0.3F, 0.3F));
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.0F, 0.5F));
  std::array<QVector3D, 2> const palette_slots{shield_red, bronze_color};
  append_equipment_archetype(batch, roman_scutum_archetype(),
                             roman_scutum_transform(ctx.model, hand_l),
                             palette_slots);
}

} // namespace Render::GL
