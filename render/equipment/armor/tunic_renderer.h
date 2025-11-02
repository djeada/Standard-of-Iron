#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct TunicConfig {
  float torso_scale = 1.06F;
  float shoulder_width_scale = 1.2F;
  float chest_depth_scale = 0.85F;
  float waist_taper = 0.92F;
  bool include_pauldrons = true;
  bool include_gorget = true;
  bool include_belt = true;
};

class TunicRenderer : public IEquipmentRenderer {
public:
  explicit TunicRenderer(const TunicConfig &config = TunicConfig{});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  TunicConfig m_config;

  void renderTorsoArmor(const DrawContext &ctx, const AttachmentFrame &torso,
                        const QVector3D &steel_color,
                        const QVector3D &brass_color, ISubmitter &submitter);

  void renderPauldrons(const DrawContext &ctx, const BodyFrames &frames,
                       const QVector3D &steel_color,
                       const QVector3D &brass_color, ISubmitter &submitter);

  void renderGorget(const DrawContext &ctx, const AttachmentFrame &torso,
                    float y_top, const QVector3D &steel_color,
                    const QVector3D &brass_color, ISubmitter &submitter);

  void renderBelt(const DrawContext &ctx, const AttachmentFrame &waist,
                  const QVector3D &steel_color, const QVector3D &brass_color,
                  ISubmitter &submitter);
};

} // namespace Render::GL
