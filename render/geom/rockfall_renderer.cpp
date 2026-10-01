#include "rockfall_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include "game/systems/render_effects_frame.h"
#include "render/scene_renderer.h"
#include "stone.h"

namespace Render::GL {

namespace {

const QVector3D k_boulder_color(0.66F, 0.63F, 0.58F);
const QVector3D k_rockfall_dust_color(0.70F, 0.64F, 0.55F);
constexpr float k_rockfall_dust_intensity = 1.5F;

} // namespace

void render_rockfall(Renderer* renderer,
                     const Game::Systems::RenderEffectsFrame& effects) {
  if (renderer == nullptr) {
    return;
  }

  if (!effects.rockfall_boulders.empty()) {
    if (auto* mesh = Geom::Stone::get(); mesh != nullptr) {
      for (auto const& boulder : effects.rockfall_boulders) {
        float const scale = boulder.radius / Geom::Stone::k_mean_radius;
        QMatrix4x4 model;
        model.translate(boulder.position - QVector3D(0.0F, boulder.sink, 0.0F));
        model.rotate(boulder.orientation);
        model.scale(scale);
        renderer->mesh(mesh, model, k_boulder_color, nullptr, 1.0F);
      }
    }
  }

  for (auto const& dust : effects.rockfall_dust) {
    renderer->stone_impact(dust.position,
                           k_rockfall_dust_color,
                           dust.scale,
                           k_rockfall_dust_intensity,
                           dust.age);
  }
}

} // namespace Render::GL
