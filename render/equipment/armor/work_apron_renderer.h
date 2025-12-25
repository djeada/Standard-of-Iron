#pragma once

#include "../../humanoid/rig.h"
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

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

  void set_config(const WorkApronConfig &config) { m_config = config; }

private:
  WorkApronConfig m_config;

  void renderApronBody(const DrawContext &ctx, const AttachmentFrame &torso,
                       const AttachmentFrame &waist, ISubmitter &submitter);

  void renderStraps(const DrawContext &ctx, const AttachmentFrame &torso,
                    const BodyFrames &frames, ISubmitter &submitter);

  void renderPockets(const DrawContext &ctx, const AttachmentFrame &waist,
                     ISubmitter &submitter);
};

} 
