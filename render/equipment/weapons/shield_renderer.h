

#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
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
  int material_id = 4;
};

class ShieldRenderer : public IEquipmentRenderer {
public:
  explicit ShieldRenderer(ShieldRenderConfig config = {});

  static void submit(const ShieldRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto
  base_config() const noexcept -> const ShieldRenderConfig & {
    return m_base;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ShieldRenderConfig m_base;
};

} // namespace Render::GL
