#include "ballista_renderer.h"

#include <QVector3D>

#include "../../ballista_geometry.h"
#include "../../registry.h"
#include "../../siege_renderer_common.h"

namespace Render::GL::Carthage {
namespace {

auto palette() -> BallistaPalette {
  BallistaPalette p;
  p.wood_frame = QVector3D(0.50F, 0.35F, 0.20F);
  p.wood_dark = QVector3D(0.35F, 0.25F, 0.15F);
  p.wood_light = QVector3D(0.60F, 0.45F, 0.28F);
  p.metal_iron = QVector3D(0.35F, 0.33F, 0.32F);
  p.accent = QVector3D(0.85F, 0.70F, 0.30F);
  p.rope = QVector3D(0.58F, 0.52F, 0.40F);
  p.leather = QVector3D(0.45F, 0.32F, 0.22F);
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
      SiegeRendererConfig{.renderer_key = "troops/carthage/ballista",
                          .default_team = QVector3D(0.4F, 0.2F, 0.6F),
                          .draw_body = &draw_ballista_body});
}

} // namespace Render::GL::Carthage
