#include "healing_beam_renderer.h"
#include "../../game/systems/healing_beam.h"
#include "../../game/systems/healing_beam_system.h"
#include "../../game/systems/healing_colors.h"
#include "../scene_renderer.h"

namespace Render::GL {

void render_healing_beams(Renderer *renderer, ResourceManager *,
                          const Game::Systems::HealingBeamSystem &beam_system) {
  if (renderer == nullptr || beam_system.get_beam_count() == 0) {
    return;
  }

  float animation_time = renderer->get_animation_time();

  for (const auto &beam : beam_system.get_beams()) {
    if (beam && beam->is_active()) {
      float intensity = beam->get_intensity();
      if (intensity < 0.01F) {
        continue;
      }

      QVector3D color = beam->get_color();
      if (Game::Systems::is_roman_healing_color(color)) {
        continue;
      }

      renderer->healing_beam(beam->get_start(), beam->get_end(), color,
                             beam->get_progress(), beam->get_beam_width(),
                             intensity, animation_time);
    }
  }
}

} 
