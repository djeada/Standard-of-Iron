

#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct SpearRenderConfig {
  QVector3D shaft_color{0.5F, 0.3F, 0.2F};
  QVector3D spearhead_color{0.70F, 0.71F, 0.76F};
  float spear_length = 1.20F;
  float shaft_radius = 0.020F;
  float spearhead_length = 0.18F;
  int material_id = 3;
};

class SpearRenderer : public IEquipmentRenderer {
public:
  explicit SpearRenderer(SpearRenderConfig config = {});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void set_config(const SpearRenderConfig &config) { m_config = config; }

private:
  SpearRenderConfig m_config;
};

} 
