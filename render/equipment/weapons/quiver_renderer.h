#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct QuiverRenderConfig {
  QVector3D leather_color{0.35F, 0.25F, 0.15F};
  QVector3D wood_color{0.30F, 0.22F, 0.14F};
  QVector3D fletching_color{0.60F, 0.20F, 0.20F};
  float quiver_radius = 0.08F;
  float quiver_height = 0.30F;
  int num_arrows = 2;
  int material_id = 3; // Material ID: 3 = weapon
};

class QuiverRenderer : public IEquipmentRenderer {
public:
  explicit QuiverRenderer(QuiverRenderConfig config = {});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void setConfig(const QuiverRenderConfig &config) { m_config = config; }

private:
  QuiverRenderConfig m_config;
};

} // namespace Render::GL
