#include "armor_heavy.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::cylinderBetween;
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

void CarthageArcherHeavyArmorRenderer::render(
    const DrawContext &ctx, const BodyFrames &frames,
    const HumanoidPalette &palette, const HumanoidAnimationContext &anim,
    ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &head = frames.head;
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;

  if (torso.radius <= 0.0F) {
    return;
  }

  const float torso_r = torso.radius;
  const QVector3D leather_color = QVector3D(0.36F, 0.24F, 0.12F);

  const QVector3D up = torso.up.normalized();
  const QVector3D right = torso.right.normalized();
  const QVector3D forward = torso.forward.normalized();

  {
    float height = torso_r * 0.95F;
    QVector3D center = torso.origin + up * (torso_r * 0.28F);

    QMatrix4x4 transform = ctx.model;
    transform.translate(center);
    transform = transform * createOrientationMatrix(right, up, forward);
    transform.scale(torso_r * 1.55F, height, torso_r * 1.25F);

    submitter.mesh(getUnitTorso(), transform, leather_color, nullptr, 0.32F);
  }

  {
    float height = torso_r * 0.75F;
    QVector3D center = torso.origin - up * (torso_r * 0.2F);

    QMatrix4x4 transform = ctx.model;
    transform.translate(center);
    transform = transform * createOrientationMatrix(right, up, forward);
    transform.scale(torso_r * 1.45F, height, torso_r * 1.1F);

    submitter.mesh(getUnitTorso(), transform, leather_color * 0.98F, nullptr,
                   0.34F);
  }

  {
    QVector3D waist_up = waist.up.normalized();
    QVector3D waist_right = waist.right.normalized();
    QVector3D waist_forward = waist.forward.normalized();

    float height = waist.radius * 1.0F;
    QVector3D center = waist.origin - waist_up * (height * 0.45F);

    QMatrix4x4 transform = ctx.model;
    transform.translate(center);
    transform =
        transform * createOrientationMatrix(waist_right, waist_up, waist_forward);
    transform.scale(waist.radius * 1.65F, height, waist.radius * 1.25F);

    submitter.mesh(getUnitTorso(), transform, leather_color * 0.97F, nullptr,
                   0.36F);
  }
}

} // namespace Render::GL
