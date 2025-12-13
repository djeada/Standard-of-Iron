#include "cloak_renderer.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

CloakRenderer::CloakRenderer(const CloakConfig &config) : m_config(config) {

  m_back_mesh.reset(createPlaneMesh(1.0F, 1.0F, 16));

  m_shoulder_mesh.reset(createPlaneMesh(1.0F, 1.0F, 12));
}

void CloakRenderer::setConfig(const CloakConfig &config) { m_config = config; }

void CloakRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &shoulder_l = frames.shoulder_l;
  const AttachmentFrame &shoulder_r = frames.shoulder_r;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D const cloak_color = palette.cloth;
  QVector3D const trim_color = palette.metal;

  QVector3D up = torso.up.normalized();
  QVector3D right = torso.right.normalized();
  QVector3D forward = torso.forward.normalized();
  QVector3D back = -forward;

  float const torso_r = torso.radius;

  float shoulder_span = (shoulder_r.origin - shoulder_l.origin).length();
  if (shoulder_span < 1e-4F) {
    shoulder_span = torso_r * 3.0F;
  }
  QVector3D shoulder_mid = (shoulder_l.origin + shoulder_r.origin) * 0.5F;

  {
    float cape_width = shoulder_span * 1.6F * m_config.width_scale;
    float cape_depth = torso_r * 1.8F;

    QVector3D cape_anchor = shoulder_mid + up * (torso_r * 0.82F);

    QMatrix4x4 cape_model;
    cape_model.translate(cape_anchor);

    float yaw = std::atan2(forward.x(), forward.z());
    cape_model.rotate(yaw * 180.0F / static_cast<float>(std::numbers::pi), 0.0F,
                      1.0F, 0.0F);

    cape_model.scale(cape_width, 1.0F, cape_depth);

    submitter.mesh(m_shoulder_mesh.get(), ctx.model * cape_model, cloak_color,
                   nullptr, 1.0F, m_config.shoulder_material_id);
  }

  {
    float drape_width = shoulder_span * 1.22F * m_config.width_scale;
    float drape_length = torso_r * 4.2F * m_config.length_scale;

    QVector3D drape_anchor =
        shoulder_mid + up * (torso_r * 0.62F) + back * (torso_r * 0.96F);

    QMatrix4x4 drape_model;
    drape_model.translate(drape_anchor);

    QMatrix4x4 drape_orient;
    drape_orient.setColumn(0, QVector4D(right, 0.0F));
    drape_orient.setColumn(1, QVector4D(back, 0.0F));
    drape_orient.setColumn(2, QVector4D(-up, 0.0F));
    drape_orient.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
    drape_model = drape_model * drape_orient;

    drape_model.translate(0.0F, 0.0F, drape_length * 0.5F);

    float const drape_bottom_flare = 0.35F;
    float const drape_shear = drape_bottom_flare * 0.35F;
    QMatrix4x4 flare;
    flare.setToIdentity();
    flare(0, 2) = drape_shear;
    drape_model = drape_model * flare;

    drape_model.scale(drape_width, 1.0F, drape_length);

    submitter.mesh(m_back_mesh.get(), ctx.model * drape_model, cloak_color,
                   nullptr, 1.0F, m_config.back_material_id);
  }

  if (m_config.show_clasp) {
    QVector3D clasp_pos =
        shoulder_mid + up * (torso_r * 0.5F) + forward * (torso_r * 0.2F);
    submitter.mesh(
        getUnitSphere(),
        Render::Geom::sphere_at(ctx.model, clasp_pos, torso_r * 0.12F),
        trim_color, nullptr, 1.0F, 1);
  }
}

} // namespace Render::GL
