#pragma once

#include "../../gl/mesh.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>
#include <memory>

namespace Render::GL {

struct CloakConfig {
  QVector3D primary_color{0.14F, 0.38F, 0.54F};
  QVector3D trim_color{0.75F, 0.66F, 0.42F};
  float length_scale = 1.0F;
  float width_scale = 1.0F;
  bool show_clasp = true;
  int back_material_id = 5;
  int shoulder_material_id = 6;
};

class CloakRenderer : public IEquipmentRenderer {
public:
  explicit CloakRenderer(const CloakConfig &config = CloakConfig{});

  void setConfig(const CloakConfig &config);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  CloakConfig m_config;
  std::unique_ptr<Mesh> m_back_mesh;
  std::unique_ptr<Mesh> m_shoulder_mesh;
};

} // namespace Render::GL
