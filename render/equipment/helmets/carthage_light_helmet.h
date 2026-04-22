#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct CarthageLightHelmetConfig {
  QVector3D bronze_color = QVector3D(0.72F, 0.45F, 0.20F);
  QVector3D leather_color = QVector3D(0.35F, 0.25F, 0.18F);
  float helmet_height = 0.18F;
  float brim_width = 0.05F;
  float cheek_guard_length = 0.12F;
  bool has_crest = true;
  bool has_nasal_guard = true;
  int detail_level = 2;
};

class CarthageLightHelmetRenderer : public IEquipmentRenderer {
public:
  CarthageLightHelmetRenderer() = default;

  static void submit(const CarthageLightHelmetConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto
  base_config() const noexcept -> const CarthageLightHelmetConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  CarthageLightHelmetConfig m_config;
};

} // namespace Render::GL
