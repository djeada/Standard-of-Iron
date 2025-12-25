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

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

RomanShieldRenderer::RomanShieldRenderer() {
  ShieldRenderConfig config;
  config.shield_color = {0.65F, 0.15F, 0.15F};
  config.trim_color = {0.78F, 0.70F, 0.45F};
  config.metal_color = {0.72F, 0.73F, 0.78F};
  config.shield_radius = 0.18F;
  config.shield_aspect = 1.3F;
  config.has_cross_decal = false;

  set_config(config);
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

  float const shield_width = 0.45F;
  float const shield_height = 1.0F;

  QVector3D shield_center = frames.hand_l.origin +
                            axis_x * (-shield_width * 0.45F) + axis_y * 0.08F +
                            n * 0.06F;

  QVector3D const shield_color{0.68F, 0.14F, 0.12F};
  QVector3D const trim_color{0.88F, 0.75F, 0.42F};
  QVector3D const metal_color{0.82F, 0.84F, 0.88F};

  QMatrix4x4 shield_body = ctx.model;
  shield_body.translate(shield_center);
  shield_body.rotate(90.0F, 0.0F, 1.0F, 0.0F);
  shield_body.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
  shield_body.scale(shield_width * 0.005F, shield_height * 0.5F, 0.24F);

  submitter.mesh(get_unit_cube(), shield_body, shield_color, nullptr, 1.0F, 4);

  float const rim_thickness = 0.020F;

  QVector3D top_left = shield_center + axis_y * (shield_height * 0.5F) -
                       axis_x * (shield_width * 0.5F);
  QVector3D top_right = shield_center + axis_y * (shield_height * 0.5F) +
                        axis_x * (shield_width * 0.5F);
  submitter.mesh(
      get_unit_cylinder(),
      cylinder_between(ctx.model, top_left, top_right, rim_thickness),
      trim_color, nullptr, 1.0F, 4);

  QVector3D bot_left = shield_center - axis_y * (shield_height * 0.5F) -
                       axis_x * (shield_width * 0.5F);
  QVector3D bot_right = shield_center - axis_y * (shield_height * 0.5F) +
                        axis_x * (shield_width * 0.5F);
  submitter.mesh(
      get_unit_cylinder(),
      cylinder_between(ctx.model, bot_left, bot_right, rim_thickness),
      trim_color, nullptr, 1.0F, 4);

  float const boss_radius = 0.08F;
  submitter.mesh(get_unit_sphere(),
                 sphere_at(ctx.model, shield_center + n * 0.05F, boss_radius),
                 metal_color, nullptr, 1.0F, 4);

  QVector3D const grip_a = shield_center - axis_x * 0.06F - n * 0.03F;
  QVector3D const grip_b = shield_center + axis_x * 0.06F - n * 0.03F;
  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, grip_a, grip_b, 0.012F),
                 palette.leather, nullptr, 1.0F, 0);
}

} 
