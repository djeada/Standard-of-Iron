#pragma once

#include "../../humanoid/rig.h"
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

  void set_config(const CarthageLightHelmetConfig &config) {
    m_config = config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  CarthageLightHelmetConfig m_config;

  void render_bowl(const DrawContext &ctx, const AttachmentFrame &head,
                   ISubmitter &submitter);
  void render_brim(const DrawContext &ctx, const AttachmentFrame &head,
                   ISubmitter &submitter);
  void render_cheek_guards(const DrawContext &ctx, const AttachmentFrame &head,
                           ISubmitter &submitter);
  void render_nasal_guard(const DrawContext &ctx, const AttachmentFrame &head,
                          ISubmitter &submitter);
  void render_crest(const DrawContext &ctx, const AttachmentFrame &head,
                    ISubmitter &submitter);
  void render_rivets(const DrawContext &ctx, const AttachmentFrame &head,
                     ISubmitter &submitter);
};

} 
