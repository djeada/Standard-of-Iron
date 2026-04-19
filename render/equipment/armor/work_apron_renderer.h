#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct WorkApronConfig {
  QVector3D leather_color = QVector3D(0.48F, 0.35F, 0.22F);
  QVector3D strap_color = QVector3D(0.35F, 0.28F, 0.20F);
  float apron_length = 0.45F;
  float apron_width = 0.65F;
  bool include_straps = true;
  bool include_pockets = true;
};

class WorkApronRenderer : public IEquipmentRenderer {
public:
  explicit WorkApronRenderer(const WorkApronConfig &config = WorkApronConfig{});

  static void submit(const WorkApronConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const WorkApronConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  WorkApronConfig m_config;

  static void renderApronBody(const WorkApronConfig &config,
                              const DrawContext &ctx,
                              const AttachmentFrame &torso,
                              const AttachmentFrame &waist,
                              EquipmentBatch &batch);

  static void renderStraps(const WorkApronConfig &config,
                           const DrawContext &ctx, const AttachmentFrame &torso,
                           const BodyFrames &frames, EquipmentBatch &batch);

  static void renderPockets(const WorkApronConfig &config,
                            const DrawContext &ctx,
                            const AttachmentFrame &waist,
                            EquipmentBatch &batch);
};

} // namespace Render::GL
