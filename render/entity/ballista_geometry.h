#pragma once

#include <QVector3D>

namespace Render::GL {

struct DrawContext;
class ISubmitter;
class Mesh;
class Texture;

struct BallistaPalette {
  QVector3D wood_frame{0.45F, 0.32F, 0.18F};
  QVector3D wood_dark{0.32F, 0.22F, 0.12F};
  QVector3D wood_light{0.55F, 0.40F, 0.25F};
  QVector3D metal_iron{0.38F, 0.36F, 0.34F};
  QVector3D accent{0.72F, 0.52F, 0.30F};
  QVector3D rope{0.62F, 0.55F, 0.42F};
  QVector3D leather{0.42F, 0.30F, 0.20F};
  QVector3D bolt{0.35F, 0.30F, 0.25F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

void draw_ballista_geometry(const DrawContext& p,
                            ISubmitter& out,
                            Mesh* unit,
                            Texture* white,
                            const QVector3D& team_color,
                            const BallistaPalette& palette);

} // namespace Render::GL
