#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct ArmGuardsConfig {
  QVector3D leather_color = QVector3D(0.50F, 0.38F, 0.26F);
  QVector3D strap_color = QVector3D(0.32F, 0.26F, 0.18F);
  float guard_length = 0.18F;
  bool include_straps = true;
};

class ArmGuardsRenderer : public IEquipmentRenderer {
public:
  explicit ArmGuardsRenderer(const ArmGuardsConfig &config = ArmGuardsConfig{});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void set_config(const ArmGuardsConfig &config) { m_config = config; }

private:
  ArmGuardsConfig m_config;

  void renderArmGuard(const DrawContext &ctx, const QVector3D &elbow,
                      const QVector3D &wrist, ISubmitter &submitter);
};

} // namespace Render::GL
