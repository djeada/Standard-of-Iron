

#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct SwordRenderConfig {
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float sword_length = 0.80F;
  float sword_width = 0.065F;
  float guard_half_width = 0.12F;
  float handle_radius = 0.016F;
  float pommel_radius = 0.045F;
  float blade_ricasso = 0.16F;
  float blade_taper_bias = 0.65F;
  bool has_scabbard = true;
  int material_id = 3;
};

class SwordRenderer : public IEquipmentRenderer {
public:
  explicit SwordRenderer(SwordRenderConfig config = {});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void set_config(const SwordRenderConfig &config) { m_config = config; }

private:
  SwordRenderConfig m_config;
};

} 
