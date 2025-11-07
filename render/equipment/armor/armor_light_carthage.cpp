#include "armor_light_carthage.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;

void ArmorLightCarthageRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D leather_color = QVector3D(0.44F, 0.30F, 0.19F);
  QVector3D leather_shadow = leather_color * 0.90F;
  QVector3D leather_highlight = leather_color * 1.08F;

  QVector3D up = torso.up.normalized();
  QVector3D right = torso.right.normalized();
  QVector3D forward = torso.forward.normalized();

  float const torso_r = torso.radius;
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.85F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.6F;

  QVector3D head_up =
      (head.up.lengthSquared() > 1e-6F) ? head.up.normalized() : up;
  QVector3D waist_up =
      (waist.up.lengthSquared() > 1e-6F) ? waist.up.normalized() : up;

  QVector3D top = torso.origin + up * (torso_r * 0.35F);
  QVector3D head_guard =
      head.origin -
      head_up * ((head_r > 0.0F ? head_r : torso_r * 0.6F) * 1.45F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom =
      waist.origin + waist_up * (waist_r * 0.03F) - forward * (torso_r * 0.01F);

  float main_radius = torso_r * 0.96F;

  QMatrix4x4 cuirass = cylinderBetween(ctx.model, top, bottom, main_radius);
  cuirass.scale(1.0F, 1.0F, 0.80F);
  submitter.mesh(getUnitTorso(), cuirass, leather_highlight, nullptr, 1.0F);

  auto strap = [&](float side) {
    QVector3D shoulder_anchor =
        top + right * (torso_r * 0.54F * side) - up * (torso_r * 0.04F);
    QVector3D chest_anchor =
        shoulder_anchor - up * (torso_r * 0.82F) + forward * (torso_r * 0.22F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, shoulder_anchor, chest_anchor,
                                   torso_r * 0.10F),
                   leather_highlight * 0.95F, nullptr, 1.0F);
  };
  strap(1.0F);
  strap(-1.0F);

  QVector3D front_panel_top =
      top + forward * (torso_r * 0.18F) - up * (torso_r * 0.06F);
  QVector3D front_panel_bottom =
      bottom + forward * (torso_r * 0.20F) + up * (torso_r * 0.03F);
  QMatrix4x4 front_panel = cylinderBetween(ctx.model, front_panel_top,
                                           front_panel_bottom, torso_r * 0.48F);
  front_panel.scale(0.95F, 1.0F, 0.32F);
  submitter.mesh(getUnitTorso(), front_panel, leather_highlight, nullptr, 1.0F);

  QVector3D back_panel_top =
      top - forward * (torso_r * 0.24F) - up * (torso_r * 0.05F);
  QVector3D back_panel_bottom =
      bottom - forward * (torso_r * 0.26F) + up * (torso_r * 0.02F);
  QMatrix4x4 back_panel = cylinderBetween(ctx.model, back_panel_top,
                                          back_panel_bottom, torso_r * 0.50F);
  back_panel.scale(0.96F, 1.0F, 0.30F);
  submitter.mesh(getUnitTorso(), back_panel, leather_shadow, nullptr, 1.0F);
}

} // namespace Render::GL
