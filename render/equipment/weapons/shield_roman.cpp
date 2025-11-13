#include "shield_roman.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

RomanShieldRenderer::RomanShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.65F, 0.15F, 0.15F};
  config.trim_color = {0.78F, 0.70F, 0.45F};
  config.metal_color = {0.72F, 0.73F, 0.78F};
  config.shield_radius = 0.18F;
  config.shield_aspect = 1.3F;
  config.has_cross_decal = false;

  setConfig(config);
}

void RomanShieldRenderer::render(const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &,
                                 ISubmitter &submitter) {
  constexpr float k_shield_yaw_degrees = -70.0F;

  QMatrix4x4 rot;
  rot.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);

  const QVector3D n = rot.map(QVector3D(0.0F, 0.0F, 1.0F));
  const QVector3D axis_x = rot.map(QVector3D(1.0F, 0.0F, 0.0F));
  const QVector3D axis_y = rot.map(QVector3D(0.0F, 1.0F, 0.0F));

  float const shield_width = 0.18F * 2.5F;
  float const shield_height = shield_width * 1.3F;

  QVector3D shield_center = frames.handL.origin +
                            axis_x * (-shield_width * 0.35F) +
                            axis_y * (-0.05F) + n * (0.06F);

  QVector3D const shield_color{0.65F, 0.15F, 0.15F};
  QVector3D const trim_color{0.78F, 0.70F, 0.45F};
  QVector3D const metal_color{0.72F, 0.73F, 0.78F};

  constexpr int width_segments = 8;
  constexpr int height_segments = 12;

  for (int h = 0; h < height_segments; ++h) {
    for (int w = 0; w < width_segments; ++w) {
      float const h_t =
          static_cast<float>(h) / static_cast<float>(height_segments - 1);
      float const w_t =
          static_cast<float>(w) / static_cast<float>(width_segments - 1);

      float const y_local = (h_t - 0.5F) * shield_height;
      float const x_local = (w_t - 0.5F) * shield_width;

      QVector3D segment_pos =
          shield_center + axis_y * y_local + axis_x * x_local;

      QMatrix4x4 m = ctx.model;
      m.translate(segment_pos);
      m.scale(0.028F, 0.032F, 0.008F);

      submitter.mesh(getUnitSphere(), m, shield_color, nullptr, 1.0F);
    }
  }

  for (int i = 0; i < width_segments + 1; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(width_segments);
    float const x_local = (t - 0.5F) * shield_width;

    QVector3D top_pos =
        shield_center + axis_y * (shield_height * 0.5F) + axis_x * x_local;
    QVector3D bot_pos =
        shield_center + axis_y * (-shield_height * 0.5F) + axis_x * x_local;

    QMatrix4x4 m_top = ctx.model;
    m_top.translate(top_pos);
    m_top.scale(0.015F);
    submitter.mesh(getUnitSphere(), m_top, trim_color, nullptr, 1.0F);

    QMatrix4x4 m_bot = ctx.model;
    m_bot.translate(bot_pos);
    m_bot.scale(0.015F);
    submitter.mesh(getUnitSphere(), m_bot, trim_color, nullptr, 1.0F);
  }

  for (int i = 0; i < height_segments + 1; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(height_segments);
    float const y_local = (t - 0.5F) * shield_height;

    QVector3D left_pos =
        shield_center + axis_y * y_local + axis_x * (-shield_width * 0.5F);
    QVector3D right_pos =
        shield_center + axis_y * y_local + axis_x * (shield_width * 0.5F);

    QMatrix4x4 m_left = ctx.model;
    m_left.translate(left_pos);
    m_left.scale(0.015F);
    submitter.mesh(getUnitSphere(), m_left, trim_color, nullptr, 1.0F);

    QMatrix4x4 m_right = ctx.model;
    m_right.translate(right_pos);
    m_right.scale(0.015F);
    submitter.mesh(getUnitSphere(), m_right, trim_color, nullptr, 1.0F);
  }

  float const boss_radius = shield_width * 0.25F;
  submitter.mesh(getUnitSphere(),
                 sphereAt(ctx.model, shield_center + n * 0.05F, boss_radius),
                 metal_color, nullptr, 1.0F);

  QVector3D const grip_a = shield_center - axis_x * 0.035F - n * 0.030F;
  QVector3D const grip_b = shield_center + axis_x * 0.035F - n * 0.030F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, grip_a, grip_b, 0.010F),
                 palette.leather, nullptr, 1.0F);
}

} // namespace Render::GL
