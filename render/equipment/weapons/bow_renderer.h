#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

enum class ArrowVisibility { Hidden, AttackCycleOnly, IdleAndAttackCycle };

struct BowRenderConfig {
  QVector3D string_color{0.30F, 0.30F, 0.32F};
  QVector3D metal_color{0.50F, 0.50F, 0.55F};
  QVector3D fletching_color{0.60F, 0.20F, 0.20F};
  float bow_rod_radius = 0.035F;
  float string_radius = 0.008F;
  float bow_depth = 0.25F;
  float bow_x = 0.0F;
  float bow_top_y = 0.0F;
  float bow_bot_y = 0.0F;
  float bow_height_scale = 1.0F;
  float bow_curve_factor = 1.0F;
  int material_id = 3;
  ArrowVisibility arrow_visibility = ArrowVisibility::AttackCycleOnly;
};

class BowRenderer : public IEquipmentRenderer {
public:
  explicit BowRenderer(BowRenderConfig config = {});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void set_config(const BowRenderConfig &config) { m_config = config; }

private:
  BowRenderConfig m_config;
};

} // namespace Render::GL
