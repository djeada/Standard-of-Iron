#pragma once

#include "../../game/core/component.h"
#include "../geom/flag.h"
#include "../gl/primitives.h"
#include "renderer_constants.h"
#include "submitter.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

// Shared flag rendering utilities for barracks entities
namespace BarracksFlagRenderer {

struct FlagColors {
  QVector3D team;
  QVector3D teamTrim;
  QVector3D timber;
  QVector3D timberLight;
  QVector3D woodDark;
};

// Draw the rally point flag if set
inline void drawRallyFlagIfAny(const DrawContext &p, ISubmitter &out,
                               Texture *white, const FlagColors &colors) {
  if (auto *prod =
          p.entity->getComponent<Engine::Core::ProductionComponent>()) {
    if (prod->rallySet && (p.resources != nullptr)) {
      auto flag = Render::Geom::Flag::create(prod->rallyX, prod->rallyZ,
                                             QVector3D(1.0F, 0.9F, 0.2F),
                                             colors.woodDark, 1.0F);
      Mesh *unit = p.resources->unit();
      out.mesh(unit, flag.pole, flag.poleColor, white, 1.0F);
      out.mesh(unit, flag.pennant, flag.pennantColor, white, 1.0F);
      out.mesh(unit, flag.finial, flag.pennantColor, white, 1.0F);
    }
  }
}

} // namespace BarracksFlagRenderer

} // namespace Render::GL
