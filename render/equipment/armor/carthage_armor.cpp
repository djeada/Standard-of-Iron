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
  (void)anim;
  (void)palette;

  // HEAVY: Steel chainmail hauberk (lorica hamata)
  // Steel gray color in range shader detects as chainmail (avgColor 0.55-0.72)
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  
  if (torso.radius <= 0.0F) return;

  float const torso_r = torso.radius;
  QVector3D top = torso.origin + torso.up * (torso_r * 0.20F);
  QVector3D bottom = waist.origin - waist.up * (torso_r * 0.35F);
  
  QMatrix4x4 mail_transform = ctx.model;
  mail_transform.translate((top + bottom) * 0.5F);
  
  // Align with torso
  QVector3D up_dir = torso.up.normalized();
  QVector3D right_dir = torso.right.normalized();
  QVector3D fwd_dir = torso.forward.normalized();
  
  QMatrix4x4 orientation;
  orientation(0, 0) = right_dir.x(); orientation(0, 1) = up_dir.x(); orientation(0, 2) = fwd_dir.x();
  orientation(1, 0) = right_dir.y(); orientation(1, 1) = up_dir.y(); orientation(1, 2) = fwd_dir.y();
  orientation(2, 0) = right_dir.z(); orientation(2, 1) = up_dir.z(); orientation(2, 2) = fwd_dir.z();
  
  mail_transform = mail_transform * orientation;
  
  float height = (top - bottom).length();
  mail_transform.scale(torso_r * 1.12F, height * 0.5F, torso_r * 1.08F);
  
  // Steel chainmail color - shader will render ring pattern
  QVector3D steel_color = QVector3D(0.65F, 0.67F, 0.70F);
  submitter.mesh(getUnitTorso(), mail_transform, steel_color, nullptr, 1.0F);
}

void CarthageLightArmorRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  // LIGHT: Linen linothorax (layered glued linen armor)
  // Cream/beige color - shader will render linen weave texture
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  
  if (torso.radius <= 0.0F) return;

  float const torso_r = torso.radius;
  QVector3D top = torso.origin + torso.up * (torso_r * 0.15F);
  QVector3D bottom = waist.origin - waist.up * (torso_r * 0.20F);
  
  QMatrix4x4 lino_transform = ctx.model;
  lino_transform.translate((top + bottom) * 0.5F);
  
  // Align with torso
  QVector3D up_dir = torso.up.normalized();
  QVector3D right_dir = torso.right.normalized();
  QVector3D fwd_dir = torso.forward.normalized();
  
  QMatrix4x4 orientation;
  orientation(0, 0) = right_dir.x(); orientation(0, 1) = up_dir.x(); orientation(0, 2) = fwd_dir.x();
  orientation(1, 0) = right_dir.y(); orientation(1, 1) = up_dir.y(); orientation(1, 2) = fwd_dir.y();
  orientation(2, 0) = right_dir.z(); orientation(2, 1) = up_dir.z(); orientation(2, 2) = fwd_dir.z();
  
  lino_transform = lino_transform * orientation;
  
  float height = (top - bottom).length();
  lino_transform.scale(torso_r * 1.06F, height * 0.5F, torso_r * 1.04F);
  
  // Cream linen color - shader will detect as linothorax and render linen weave
  QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);
  submitter.mesh(getUnitTorso(), lino_transform, linen_color, nullptr, 1.0F);
}

} // namespace Render::GL
