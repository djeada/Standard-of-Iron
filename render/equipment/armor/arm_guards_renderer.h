#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
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

  static void submit(const ArmGuardsConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const ArmGuardsConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ArmGuardsConfig m_config;

  static void renderArmGuard(const ArmGuardsConfig &config,
                             const DrawContext &ctx, const QVector3D &elbow,
                             const QVector3D &wrist, EquipmentBatch &batch);
};

} // namespace Render::GL
