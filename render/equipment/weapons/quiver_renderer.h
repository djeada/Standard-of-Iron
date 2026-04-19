#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
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
  int material_id = 3;
};

class QuiverRenderer : public IEquipmentRenderer {
public:
  explicit QuiverRenderer(QuiverRenderConfig config = {});

  static void submit(const QuiverRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  void set_config(const QuiverRenderConfig &config) { m_config = config; }

  [[nodiscard]] auto base_config() const -> const QuiverRenderConfig & {
    return m_config;
  }

private:
  QuiverRenderConfig m_config;
};

} // namespace Render::GL
