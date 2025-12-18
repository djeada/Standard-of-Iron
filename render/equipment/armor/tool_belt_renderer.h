#pragma once

#include "../../humanoid/rig.h"
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

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void set_config(const ToolBeltConfig &config) { m_config = config; }

private:
  ToolBeltConfig m_config;

  void renderBelt(const DrawContext &ctx, const AttachmentFrame &waist,
                  ISubmitter &submitter);

  void renderHammerLoop(const DrawContext &ctx, const AttachmentFrame &waist,
                        ISubmitter &submitter);

  void renderChiselHolder(const DrawContext &ctx, const AttachmentFrame &waist,
                          ISubmitter &submitter);

  void renderPouches(const DrawContext &ctx, const AttachmentFrame &waist,
                     ISubmitter &submitter);
};

} // namespace Render::GL
