#include "roman_scutum.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

void RomanScutumRenderer::render(const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &anim,
                                 ISubmitter &submitter) {
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
  QVector3D const wood_color = saturate_color(QVector3D(0.5F, 0.35F, 0.25F));

  constexpr float shield_height = 1.2F;
  constexpr float shield_width = 0.65F;
  constexpr float shield_curve = 0.25F;
  constexpr float rim_thickness = 0.015F;
  constexpr float boss_radius = 0.12F;

  QVector3D const shield_center = hand_l.origin + hand_l.forward * 0.15F;
  QVector3D const shield_up = hand_l.up;
  QVector3D const shield_right = hand_l.right;
  QVector3D const shield_forward = hand_l.forward;

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

      QVector3D segment_pos = shield_center + shield_up * y_local +
                              shield_right * x_local +
                              shield_forward * curve_offset;

      QMatrix4x4 m = ctx.model;
      m.translate(segment_pos);
      m.scale(0.03F, 0.05F, 0.01F);

      QVector3D segment_color = shield_red * (1.0F + (v % 2) * 0.05F - 0.025F);
      submitter.mesh(get_unit_sphere(), m, segment_color, nullptr, 1.0F, 4);
    }
  }

  constexpr int ridge_segments = 10;
  for (int i = 0; i < ridge_segments; ++i) {
    float const t =
        static_cast<float>(i) / static_cast<float>(ridge_segments - 1);
    float const y_local = (t - 0.5F) * shield_height * 0.9F;

    QVector3D ridge_pos = shield_center + shield_up * y_local +
                          shield_forward * (shield_curve + 0.02F);

    QMatrix4x4 m = ctx.model;
    m.translate(ridge_pos);
    m.scale(0.025F, 0.06F, 0.015F);
    submitter.mesh(get_unit_sphere(), m, bronze_color * 0.9F, nullptr, 1.0F, 4);
  }

  QVector3D const boss_center =
      shield_center + shield_forward * (shield_curve + 0.08F);

  for (int i = 0; i < 12; ++i) {
    float const angle =
        (static_cast<float>(i) / 12.0F) * 2.0F * std::numbers::pi_v<float>;
    QVector3D ring_pos = boss_center +
                         shield_right * (boss_radius * std::cos(angle)) +
                         shield_up * (boss_radius * std::sin(angle));

    QMatrix4x4 m = ctx.model;
    m.translate(ring_pos);
    m.scale(0.018F);
    submitter.mesh(get_unit_sphere(), m, bronze_color, nullptr, 1.0F, 4);
  }

  submitter.mesh(get_unit_sphere(),
                 sphere_at(ctx.model, boss_center, boss_radius * 0.8F),
                 bronze_color * 1.1F, nullptr, 1.0F, 4);

  float const y_pos = shield_height * 0.48F;
  for (int i = 0; i < 10; ++i) {
    float const t = static_cast<float>(i) / 9.0F;
    float const x_local = (t - 0.5F) * shield_width * 0.95F;
    float const curve_off =
        shield_curve * (1.0F - std::abs(x_local / (shield_width * 0.5F)));

    QVector3D rim_pos = shield_center + shield_up * y_pos +
                        shield_right * x_local + shield_forward * curve_off;
    QMatrix4x4 m = ctx.model;
    m.translate(rim_pos);
    m.scale(rim_thickness);
    submitter.mesh(get_unit_sphere(), m, bronze_color * 0.95F, nullptr, 1.0F,
                   4);
  }

  float const y_pos_bot = -shield_height * 0.48F;
  for (int i = 0; i < 10; ++i) {
    float const t = static_cast<float>(i) / 9.0F;
    float const x_local = (t - 0.5F) * shield_width * 0.95F;
    float const curve_off =
        shield_curve * (1.0F - std::abs(x_local / (shield_width * 0.5F)));

    QVector3D rim_pos = shield_center + shield_up * y_pos_bot +
                        shield_right * x_local + shield_forward * curve_off;
    QMatrix4x4 m = ctx.model;
    m.translate(rim_pos);
    m.scale(rim_thickness);
    submitter.mesh(get_unit_sphere(), m, bronze_color * 0.95F, nullptr, 1.0F,
                   4);
  }

  for (int side = 0; side < 2; ++side) {
    float const x_pos_side = (side == 0 ? -1.0F : 1.0F) * shield_width * 0.48F;
    float const curve_off =
        shield_curve * (1.0F - std::abs(x_pos_side / (shield_width * 0.5F)));

    for (int i = 0; i < 12; ++i) {
      float const t = static_cast<float>(i) / 11.0F;
      float const y_local = (t - 0.5F) * shield_height * 0.95F;

      QVector3D rim_pos = shield_center + shield_up * y_local +
                          shield_right * x_pos_side +
                          shield_forward * curve_off;
      QMatrix4x4 m = ctx.model;
      m.translate(rim_pos);
      m.scale(rim_thickness);
      submitter.mesh(get_unit_sphere(), m, bronze_color * 0.95F, nullptr, 1.0F,
                     4);
    }
  }

  for (int i = 0; i < 8; ++i) {
    float const angle =
        (static_cast<float>(i) / 8.0F) * 2.0F * std::numbers::pi_v<float>;
    float const rivet_dist = boss_radius * 1.3F;
    QVector3D rivet_pos = boss_center +
                          shield_right * (rivet_dist * std::cos(angle)) +
                          shield_up * (rivet_dist * std::sin(angle));

    QMatrix4x4 m = ctx.model;
    m.translate(rivet_pos);
    m.scale(0.012F);
    submitter.mesh(get_unit_sphere(), m, bronze_color * 1.15F, nullptr, 1.0F,
                   4);
  }
}

} 
