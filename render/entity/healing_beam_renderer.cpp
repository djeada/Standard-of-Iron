#include "healing_beam_renderer.h"
#include "../../game/systems/healing_beam_system.h"
#include "../gl/backend.h"
#include "../gl/backend/healing_beam_pipeline.h"
#include "../gl/camera.h"
#include "../scene_renderer.h"
#include <QDebug>

namespace Render::GL {

void render_healing_beams(Renderer *renderer, ResourceManager *,
                          const Game::Systems::HealingBeamSystem &beam_system) {
  if (renderer == nullptr || beam_system.get_beam_count() == 0) {
    return;
  }

  auto *backend = renderer->backend();
  auto *camera = renderer->camera();
  if (backend == nullptr || camera == nullptr) {
    return;
  }

  auto *pipeline = backend->healing_beam_pipeline();
  if (pipeline == nullptr) {
    qWarning() << "HealingBeamPipeline not available";
    return;
  }

  pipeline->render(&beam_system, *camera, renderer->get_animation_time());
}

} // namespace Render::GL
