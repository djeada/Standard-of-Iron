#pragma once

#include <QVector3D>

#include "registry.h"

namespace Render::GL {

class Mesh;
class Texture;

struct StockpileYardStyle {
  QVector3D gravel{0.42F, 0.38F, 0.31F};
  QVector3D earth_light{0.54F, 0.48F, 0.39F};
  QVector3D stone_light{0.76F, 0.73F, 0.66F};
  QVector3D stone_mid{0.62F, 0.59F, 0.53F};
  QVector3D stone_dark{0.44F, 0.42F, 0.38F};
  QVector3D timber{0.47F, 0.32F, 0.18F};
  QVector3D timber_dark{0.29F, 0.19F, 0.11F};
  QVector3D ore{0.30F, 0.29F, 0.31F};
  QVector3D ore_rust{0.44F, 0.27F, 0.16F};
  QVector3D sack{0.80F, 0.70F, 0.50F};
  QVector3D grain{0.90F, 0.76F, 0.40F};
};

void draw_barracks_stockpile(const DrawContext& ctx,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const StockpileYardStyle& style);

} // namespace Render::GL
