#include "kingdom_armor.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include "tunic_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::GL::Humanoid::saturate_color;

void KingdomHeavyArmorRenderer::render(const DrawContext &ctx,
                                       const BodyFrames &frames,
                                       const HumanoidPalette &palette,
                                       const HumanoidAnimationContext &anim,
                                       ISubmitter &submitter) {

  TunicConfig config;
  config.torso_scale = 1.08F;
  config.shoulder_width_scale = 1.25F;
  config.chest_depth_scale = 0.85F;
  config.waist_taper = 0.92F;
  config.include_pauldrons = true;
  config.include_gorget = true;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);

  using HP = HumanProportions;
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.1F, 0.7F));
  QVector3D const chainmail_color =
      saturate_color(palette.metal * QVector3D(0.85F, 0.88F, 0.92F));
  QVector3D const mantling_color = palette.cloth;

  float const y_neck = frames.head.origin.y() - HP::HEAD_RADIUS * 0.6F;
  for (int i = 0; i < 5; ++i) {
    float const y = y_neck - static_cast<float>(i) * 0.022F;
    float const r = HP::NECK_RADIUS * (1.85F + static_cast<float>(i) * 0.08F);
    QVector3D const ring_pos(frames.torso.origin.x(), y,
                             frames.torso.origin.z());
    QVector3D const a = ring_pos + QVector3D(0, 0.010F, 0);
    QVector3D const b = ring_pos - QVector3D(0, 0.010F, 0);
    submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                   chainmail_color * (1.0F - static_cast<float>(i) * 0.04F),
                   nullptr, 1.0F);
  }

  QVector3D const helm_top =
      frames.head.origin + QVector3D(0, HP::HEAD_RADIUS * 0.85F, 0);
  QMatrix4x4 crest_base = ctx.model;
  crest_base.translate(helm_top);
  crest_base.scale(0.025F, 0.015F, 0.025F);
  submitter.mesh(getUnitSphere(), crest_base, brass_color * 1.2F, nullptr,
                 1.0F);

  auto draw_stud = [&](const QVector3D &pos) {
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(0.008F);
    submitter.mesh(getUnitSphere(), m, brass_color * 1.3F, nullptr, 1.0F);
  };

  draw_stud(helm_top + QVector3D(0.020F, 0, 0.020F));
  draw_stud(helm_top + QVector3D(-0.020F, 0, 0.020F));
  draw_stud(helm_top + QVector3D(0.020F, 0, -0.020F));
  draw_stud(helm_top + QVector3D(-0.020F, 0, -0.020F));
}

void KingdomLightArmorRenderer::render(const DrawContext &ctx,
                                       const BodyFrames &frames,
                                       const HumanoidPalette &palette,
                                       const HumanoidAnimationContext &anim,
                                       ISubmitter &submitter) {

  TunicConfig config;
  config.torso_scale = 1.04F;
  config.shoulder_width_scale = 1.15F;
  config.chest_depth_scale = 0.88F;
  config.waist_taper = 0.94F;
  config.include_pauldrons = false;
  config.include_gorget = false;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);

  using HP = HumanProportions;
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.1F, 0.7F));

  QVector3D const shoulderL_pos = frames.shoulderL.origin;
  QVector3D const shoulderR_pos = frames.shoulderR.origin;

  auto draw_phalera = [&](const QVector3D &pos) {
    QMatrix4x4 m = ctx.model;
    m.translate(pos + QVector3D(0, 0.05F, 0.02F));
    m.scale(0.020F);
    submitter.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
  };

  draw_phalera(shoulderL_pos);
  draw_phalera(shoulderR_pos);
}

} // namespace Render::GL
