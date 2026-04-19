#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct ToolBeltConfig {
  QVector3D leather_color = QVector3D(0.52F, 0.40F, 0.28F);
  QVector3D metal_color = QVector3D(0.60F, 0.58F, 0.56F);
  QVector3D wood_color = QVector3D(0.45F, 0.35F, 0.22F);
  bool include_hammer = true;
  bool include_chisel = true;
  bool include_pouches = true;
};

class ToolBeltRenderer : public IEquipmentRenderer {
public:
  explicit ToolBeltRenderer(const ToolBeltConfig &config = ToolBeltConfig{});

  static void submit(const ToolBeltConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const ToolBeltConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ToolBeltConfig m_config;

  static void renderBelt(const ToolBeltConfig &config, const DrawContext &ctx,
                         const AttachmentFrame &waist, EquipmentBatch &batch);

  static void renderHammerLoop(const ToolBeltConfig &config,
                               const DrawContext &ctx,
                               const AttachmentFrame &waist,
                               EquipmentBatch &batch);

  static void renderChiselHolder(const ToolBeltConfig &config,
                                 const DrawContext &ctx,
                                 const AttachmentFrame &waist,
                                 EquipmentBatch &batch);

  static void renderPouches(const ToolBeltConfig &config,
                            const DrawContext &ctx,
                            const AttachmentFrame &waist,
                            EquipmentBatch &batch);
};

} // namespace Render::GL
