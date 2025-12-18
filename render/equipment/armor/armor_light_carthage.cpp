#include "armor_light_carthage.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::GL::Humanoid::saturate_color;

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

  QVector3D leather_color = saturate_color(palette.leather);
  QVector3D leather_shadow =
      saturate_color(leather_color * QVector3D(0.90F, 0.90F, 0.90F));
  QVector3D leather_highlight =
      saturate_color(leather_color * QVector3D(1.08F, 1.05F, 1.02F));
  QVector3D metal_color =
      saturate_color(palette.metal * QVector3D(1.00F, 0.94F, 0.88F));
  QVector3D metal_core =
      saturate_color(metal_color * QVector3D(0.94F, 0.94F, 0.94F));
  QVector3D cloth_accent =
      saturate_color(palette.cloth * QVector3D(1.05F, 1.02F, 1.04F));

  QVector3D up = torso.up.normalized();
  QVector3D right = torso.right.normalized();
  QVector3D forward = torso.forward.normalized();

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.85F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.6F;

  QVector3D head_up =
      (head.up.lengthSquared() > 1e-6F) ? head.up.normalized() : up;
  QVector3D waist_up =
      (waist.up.lengthSquared() > 1e-6F) ? waist.up.normalized() : up;

  QVector3D top = torso.origin + up * (torso_r * 0.50F);
  QVector3D head_guard =
      head.origin -
      head_up * ((head_r > 0.0F ? head_r : torso_r * 0.6F) * 1.45F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.24F) -
                     forward * (torso_r * 0.016F);

  float main_radius = torso_r * 1.36F;
  float const main_depth = torso_depth * 1.24F;

  QMatrix4x4 cuirass = cylinder_between(ctx.model, top, bottom, main_radius);
  cuirass.scale(1.0F, 1.0F, std::max(0.15F, main_depth / main_radius));
  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(), cuirass,
                 metal_color, nullptr, 1.0F, 1);

  auto strap = [&](float side) {
    QVector3D shoulder_anchor =
        top + right * (torso_r * 0.48F * side) - up * (torso_r * 0.12F);
    QVector3D chest_anchor =
        shoulder_anchor - up * (torso_r * 0.82F) + forward * (torso_r * 0.22F);
    submitter.mesh(get_unit_cylinder(),
                   cylinder_between(ctx.model, shoulder_anchor, chest_anchor,
                                    torso_r * 0.12F),
                   leather_highlight * 0.95F, nullptr, 1.0F, 1);
  };
  strap(1.0F);
  strap(-1.0F);

  QVector3D front_panel_top =
      top + forward * (torso_depth * 0.35F) - up * (torso_r * 0.06F);
  QVector3D front_panel_bottom =
      bottom + forward * (torso_depth * 0.38F) + up * (torso_r * 0.03F);
  QMatrix4x4 front_panel = cylinder_between(
      ctx.model, front_panel_top, front_panel_bottom, torso_r * 0.56F);
  front_panel.scale(1.18F, 1.0F,
                    std::max(0.22F, (torso_depth * 0.76F) / (torso_r * 0.76F)));
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(),
                 front_panel, cloth_accent, nullptr, 1.0F, 1);

  QVector3D back_panel_top =
      top - forward * (torso_depth * 0.32F) - up * (torso_r * 0.05F);
  QVector3D back_panel_bottom =
      bottom - forward * (torso_depth * 0.34F) + up * (torso_r * 0.02F);
  QMatrix4x4 back_panel = cylinder_between(ctx.model, back_panel_top,
                                           back_panel_bottom, torso_r * 0.58F);
  back_panel.scale(1.18F, 1.0F,
                   std::max(0.22F, (torso_depth * 0.74F) / (torso_r * 0.80F)));
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(),
                 back_panel, metal_core, nullptr, 1.0F, 1);
}

} // namespace Render::GL
