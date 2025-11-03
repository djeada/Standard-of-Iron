#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct ChainmailArmorConfig {
  QVector3D metal_color = QVector3D(0.65F, 0.67F, 0.70F); // Steel gray
  QVector3D rust_tint = QVector3D(0.52F, 0.35F, 0.25F);
  float coverage = 1.0F; // 0.0 = minimal, 1.0 = full coverage
  float rust_amount = 0.15F; // 0.0 = pristine, 1.0 = heavily rusted
  float ring_size = 0.008F;
  bool has_shoulder_guards = true;
  bool has_arm_coverage = true;
  int detail_level = 2; // 0=low (simple), 1=medium, 2=high (individual rings)
};

class ChainmailArmorRenderer : public IEquipmentRenderer {
public:
  ChainmailArmorRenderer() = default;

  void setConfig(const ChainmailArmorConfig &config) { m_config = config; }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  ChainmailArmorConfig m_config;

  void renderTorsoMail(const DrawContext &ctx, const BodyFrames &frames,
                       ISubmitter &submitter);
  void renderShoulderGuards(const DrawContext &ctx, const BodyFrames &frames,
                            ISubmitter &submitter);
  void renderArmMail(const DrawContext &ctx, const BodyFrames &frames,
                     ISubmitter &submitter);
  void renderRingDetails(const DrawContext &ctx, const QVector3D &center,
                         float radius, float height, const QVector3D &up,
                         const QVector3D &right, ISubmitter &submitter);
  
  auto calculateRingColor(float x, float y, float z) const -> QVector3D;
};

} // namespace Render::GL
