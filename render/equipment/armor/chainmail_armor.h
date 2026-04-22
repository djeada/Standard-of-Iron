#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct ChainmailArmorConfig {
  QVector3D metal_color = QVector3D(0.65F, 0.67F, 0.70F);
  QVector3D rust_tint = QVector3D(0.52F, 0.35F, 0.25F);
  float coverage = 1.0F;
  float rust_amount = 0.15F;
  float ring_size = 0.008F;
  bool has_shoulder_guards = true;
  bool has_arm_coverage = true;
  int detail_level = 2;
};

class ChainmailArmorRenderer : public IEquipmentRenderer {
public:
  ChainmailArmorRenderer() = default;

  static void submit(const ChainmailArmorConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto
  base_config() const noexcept -> const ChainmailArmorConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ChainmailArmorConfig m_config;

  static auto calculate_ring_color(const ChainmailArmorConfig &config, float x,
                                   float y, float z) -> QVector3D;
};

} // namespace Render::GL
