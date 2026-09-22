#pragma once

#include <vector>

namespace Game::Systems {
struct HealingBeamView;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

void render_healing_beams(Renderer* renderer,
                          ResourceManager* resources,
                          const std::vector<Game::Systems::HealingBeamView>& beams);

} // namespace Render::GL
