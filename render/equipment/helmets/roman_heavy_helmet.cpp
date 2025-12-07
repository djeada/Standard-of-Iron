#include "roman_heavy_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

namespace {

constexpr float helm_scale = 1.18f;
constexpr float cap_scale = 0.96f;
constexpr float brow_scale = 1.10f;

constexpr float helmet_bottom_y = -0.25f;
constexpr float helmet_top_y = 1.42f;
constexpr float cap_top_y = 1.52f;

constexpr float brow_center_y = 0.12f;
constexpr float brow_top_offset = 0.035f;
constexpr float brow_bottom_offset = 0.025f;
constexpr float neck_scale = 0.98f;
constexpr float neck_top_y = -0.12f;
constexpr float neck_top_z = -1.08f;
constexpr float neck_bottom_y = -0.35f;
constexpr float neck_bottom_z = -1.02f;

constexpr float crest_mid_offset = 0.10f;
constexpr float crest_top_offset = 0.18f;
constexpr float crest_mount_radius = 0.022f;
constexpr float crest_cone_radius = 0.052f;
constexpr float crest_top_sphere_r = 0.024f;

constexpr float steel_color_mul[3] = {0.88f, 0.92f, 1.08f};
constexpr float brass_color_mul[3] = {1.40f, 1.15f, 0.65f};

constexpr float helmet_y_offset = 0.05f;

} // namespace

void RomanHeavyHelmetRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      ISubmitter &submitter) {
  (void)anim;

  AttachmentFrame head = frames.head;
  float head_r = head.radius;
  if (head_r <= 0.0f) {
    return;
  }

  auto head_point = [&](const QVector3D &n) {
    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D steel_color = saturate_color(
      palette.metal *
      QVector3D(steel_color_mul[0], steel_color_mul[1], steel_color_mul[2]));

  QVector3D brass_color = saturate_color(
      palette.metal *
      QVector3D(brass_color_mul[0], brass_color_mul[1], brass_color_mul[2]));

  float helm_r = head_r * helm_scale;

  QVector3D helm_bot = head_point({0.0f, helmet_bottom_y, 0.0f});
  QVector3D helm_top = head_point({0.0f, helmet_top_y, 0.0f});

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_bot, helm_top, helm_r),
                 steel_color, nullptr, 1.0f, 2);

  QVector3D cap_top = head_point({0.0f, cap_top_y, 0.0f});
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, helm_top, cap_top, helm_r * cap_scale),
      steel_color * 1.06f, nullptr, 1.0f, 2);

  QVector3D brow_center = head_point({0.0f, brow_center_y, 0.0f});
  QVector3D brow_top = brow_center + head.up * brow_top_offset;
  QVector3D brow_bot = brow_center - head.up * brow_bottom_offset;

  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, brow_bot, brow_top, helm_r * brow_scale),
      brass_color * 0.92f, nullptr, 1.0f, 2);

  QVector3D neck_top = head_point({0.0f, neck_top_y, neck_top_z});
  QVector3D neck_bot = head_point({0.0f, neck_bottom_y, neck_bottom_z});

  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, neck_bot, neck_top, helm_r * neck_scale),
      steel_color * 0.88f, nullptr, 1.0f, 2);

  QVector3D crest_base = cap_top;
  QVector3D crest_mid = crest_base + head.up * crest_mid_offset;
  QVector3D crest_top = crest_mid + head.up * crest_top_offset;

  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, crest_base, crest_mid, crest_mount_radius),
      brass_color, nullptr, 1.0f, 2);

  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, crest_mid, crest_top, crest_cone_radius),
                 QVector3D(0.96f, 0.12f, 0.12f), nullptr, 1.0f, 0);

  submitter.mesh(getUnitSphere(),
                 sphereAt(ctx.model, crest_top, crest_top_sphere_r),
                 brass_color, nullptr, 1.0f, 2);
}

} // namespace Render::GL
