#include "ballista_renderer.h"

#include <QVector3D>

#include "render/entity/ballista_geometry.h"
#include "render/entity/registry.h"
#include "render/entity/siege_renderer_common.h"

namespace Render::GL::Roman {
namespace {

auto palette() -> BallistaPalette {
  BallistaPalette p;
  p.wood_frame = QVector3D(0.45F, 0.32F, 0.18F);
  p.wood_dark = QVector3D(0.32F, 0.22F, 0.12F);
  p.wood_light = QVector3D(0.55F, 0.40F, 0.25F);
  p.metal_iron = QVector3D(0.38F, 0.36F, 0.34F);
  p.accent = QVector3D(0.72F, 0.52F, 0.30F);
  p.rope = QVector3D(0.62F, 0.55F, 0.42F);
  p.leather = QVector3D(0.42F, 0.30F, 0.20F);
  return p;
}

void draw_ballista_body(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const QVector3D& team_color) {
  draw_ballista_geometry(p, out, unit, white, team_color, palette());
}

} // namespace

void register_ballista_renderer(EntityRendererRegistry& registry) {
  register_siege_renderer_variant(
      registry,
      SiegeRendererConfig{.renderer_key = "troops/roman/ballista",
                          .default_team = QVector3D(0.8F, 0.2F, 0.2F),
                          .draw_body = &draw_ballista_body});
}

} // namespace Render::GL::Roman
