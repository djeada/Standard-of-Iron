#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct ShieldRenderConfig {
  QVector3D shield_color{0.7F, 0.3F, 0.2F};
  QVector3D trim_color{0.72F, 0.73F, 0.78F};
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float shield_radius = 0.18F;
  float shield_aspect = 1.0F;
  bool has_cross_decal = false;
};

class ShieldRenderer : public IEquipmentRenderer {
public:
  explicit ShieldRenderer(ShieldRenderConfig config = {});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void setConfig(const ShieldRenderConfig &config) { m_config = config; }

private:
  ShieldRenderConfig m_config;
};

} // namespace Render::GL
