#include "carthage_armor.h"
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

void CarthageHeavyArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {

  TunicConfig config;
  config.torso_scale = 1.07F;
  config.shoulder_width_scale = 1.22F;
  config.chest_depth_scale = 0.84F;
  config.waist_taper = 0.91F;
  config.include_pauldrons = true;
  config.include_gorget = true;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);

  using HP = HumanProportions;
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.2F, 1.0F, 0.65F));
  QVector3D const chainmail_color =
      saturate_color(palette.metal * QVector3D(0.82F, 0.85F, 0.90F));

  float const y_neck = frames.head.origin.y() - HP::HEAD_RADIUS * 0.6F;
  for (int i = 0; i < 3; ++i) {
    float const y = y_neck - static_cast<float>(i) * 0.025F;
    float const r = HP::NECK_RADIUS * (1.80F + static_cast<float>(i) * 0.10F);
    QVector3D const ring_pos(frames.torso.origin.x(), y,
                             frames.torso.origin.z());
    QVector3D const a = ring_pos + QVector3D(0, 0.012F, 0);
    QVector3D const b = ring_pos - QVector3D(0, 0.012F, 0);
    submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                   chainmail_color * (1.0F - static_cast<float>(i) * 0.05F),
                   nullptr, 1.0F);
  }

  auto draw_phalera = [&](const QVector3D &pos) {
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(0.025F);
    submitter.mesh(getUnitSphere(), m, bronze_color, nullptr, 1.0F);
  };

  draw_phalera(frames.shoulderL.origin + QVector3D(0, 0.05F, 0.02F));
  draw_phalera(frames.shoulderR.origin + QVector3D(0, 0.05F, 0.02F));
}

void CarthageLightArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {

  TunicConfig config;
  config.torso_scale = 1.03F;
  config.shoulder_width_scale = 1.14F;
  config.chest_depth_scale = 0.89F;
  config.waist_taper = 0.95F;
  config.include_pauldrons = false;
  config.include_gorget = false;
  config.include_belt = true;

  TunicRenderer renderer(config);
  renderer.render(ctx, frames, palette, anim, submitter);

  using HP = HumanProportions;
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.1F, 0.9F, 0.6F));

  auto draw_ornament = [&](const QVector3D &pos) {
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(0.015F);
    submitter.mesh(getUnitSphere(), m, bronze_color, nullptr, 1.0F);
  };

  draw_ornament(frames.shoulderL.origin + QVector3D(0, 0.04F, 0.02F));
  draw_ornament(frames.shoulderR.origin + QVector3D(0, 0.04F, 0.02F));
}

} // namespace Render::GL
