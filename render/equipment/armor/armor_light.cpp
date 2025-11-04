#include "armor_light.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::sphereAt;

namespace {
QMatrix4x4 createOrientationMatrix(const QVector3D &right, const QVector3D &up,
                                   const QVector3D &forward) {
  QMatrix4x4 orientation;
  orientation(0, 0) = right.x();
  orientation(0, 1) = up.x();
  orientation(0, 2) = forward.x();
  orientation(1, 0) = right.y();
  orientation(1, 1) = up.y();
  orientation(1, 2) = forward.y();
  orientation(2, 0) = right.z();
  orientation(2, 1) = up.z();
  orientation(2, 2) = forward.z();
  return orientation;
}
} // namespace

void CarthageArcherLightArmorRenderer::render(
    const DrawContext &ctx, const BodyFrames &frames,
    const HumanoidPalette &palette, const HumanoidAnimationContext &anim,
    ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);

  const QVector3D up = torso.up.normalized();
  const QVector3D right = torso.right.normalized();
  const QVector3D forward = torso.forward.normalized();

  QVector3D top = torso.origin + up * (torso.radius * 0.12F);
  if (head.radius > 0.0F) {
    top = head.origin - head.up.normalized() * (head.radius * 0.35F);
  }

  QVector3D bottom = torso.origin - up * (torso.radius * 0.30F);
  if (waist.radius > 0.0F) {
    bottom = waist.origin - waist.up.normalized() * (waist.radius * 0.28F);
  }

  QVector3D center = (top + bottom) * 0.5F;
  float height = (top - bottom).length();

  float torso_radius = torso.radius;
  float width = torso_radius * 1.05F;
  float depth = torso_radius * 1.02F;

  QMatrix4x4 transform = ctx.model;
  transform.translate(center);
  transform = transform * createOrientationMatrix(right, up, forward);
  transform.scale(width, height * 0.48F, depth);

  submitter.mesh(getUnitTorso(), transform, linen_color, nullptr, 0.8F);
}

} // namespace Render::GL
