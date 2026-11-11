#pragma once

#include <QVector3D>

namespace Render::GL {

struct DrawContext;
class ISubmitter;
class BuildingArchetypeDesc;

struct BarracksGrounds {
  QVector3D parade_centre{-2.85F, 0.0F, 0.20F};
  float parade_half_x{0.92F};
  float parade_half_z{1.30F};
  QVector3D smithy_centre{-0.20F, 0.0F, -2.25F};
};

struct BarracksGroundsStyle {
  QVector3D earth{0.52F, 0.43F, 0.31F};
  QVector3D earth_dark{0.40F, 0.33F, 0.24F};
  QVector3D sand{0.74F, 0.66F, 0.50F};
  QVector3D stone{0.62F, 0.60F, 0.56F};
  QVector3D stone_dark{0.38F, 0.37F, 0.35F};
  QVector3D timber{0.43F, 0.35F, 0.28F};
  QVector3D timber_dark{0.27F, 0.22F, 0.18F};
  QVector3D roof{0.74F, 0.40F, 0.27F};
  QVector3D roof_dark{0.56F, 0.29F, 0.20F};
  QVector3D iron{0.24F, 0.24F, 0.26F};
  QVector3D straw{0.78F, 0.66F, 0.38F};
  QVector3D team_cloth{0.60F, 0.12F, 0.10F};
};

void add_barracks_grounds(BuildingArchetypeDesc& desc,
                          const BarracksGrounds& grounds,
                          const BarracksGroundsStyle& style);

void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds);

} // namespace Render::GL
