#include "healing_beam_renderer.h"

#include "game/systems/healing_colors.h"
#include "game/systems/render_effects_frame.h"
#include "render/scene_renderer.h"

namespace Render::GL {

void render_healing_beams(Renderer* renderer,
                          ResourceManager*,
                          const std::vector<Game::Systems::HealingBeamView>& beams) {
  if (renderer == nullptr || beams.empty()) {
    return;
  }

  float animation_time = renderer->get_animation_time();

  for (const auto& beam : beams) {
    if (beam.intensity < 0.01F) {
      continue;
    }
    if (Game::Systems::is_roman_healing_color(beam.color)) {
      continue;
    }

    renderer->healing_beam(beam.start,
                           beam.end,
                           beam.color,
                           beam.progress,
                           beam.beam_width,
                           beam.intensity,
                           animation_time);
  }
}

} // namespace Render::GL
