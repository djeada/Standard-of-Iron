#pragma once
#include "../../humanoid/rig.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct CarthageHeavyHelmetConfig {
  QVector3D bronze_color = QVector3D(0.72F, 0.45F, 0.20F);
  QVector3D crest_color = QVector3D(0.95F, 0.95F, 0.90F);
  QVector3D glow_color = QVector3D(1.0F, 0.98F, 0.92F);
  bool has_cheek_guards = true;
  bool has_face_plate = true;
  bool has_neck_guard = true;
  bool has_hair_crest = true;
  int detail_level = 2;
};

class CarthageHeavyHelmetRenderer : public IEquipmentRenderer {
public:
  explicit CarthageHeavyHelmetRenderer(
      const CarthageHeavyHelmetConfig &cfg = {})
      : m_config(cfg) {}

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  CarthageHeavyHelmetConfig m_config;

  void render_bowl(const DrawContext &ctx, const AttachmentFrame &head,
                   ISubmitter &submitter);
  void render_cheek_guards(const DrawContext &ctx, const AttachmentFrame &head,
                           ISubmitter &submitter);
  void render_face_plate(const DrawContext &ctx, const AttachmentFrame &head,
                         ISubmitter &submitter);
  void render_neck_guard(const DrawContext &ctx, const AttachmentFrame &head,
                         ISubmitter &submitter);
  void render_brow_arch(const DrawContext &ctx, const AttachmentFrame &head,
                        ISubmitter &submitter);
  void render_crest(const DrawContext &ctx, const AttachmentFrame &head,
                    ISubmitter &submitter);
};

} // namespace Render::GL
