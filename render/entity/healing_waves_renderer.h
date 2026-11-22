#pragma once

#include <vector>

namespace Game::Systems {
struct HealingBeamView;
}

namespace Render::GL {
class FrameSubmitter;
class DrawResources;

void render_healing_waves(FrameSubmitter* renderer,
                          const DrawResources* resources,
                          const std::vector<Game::Systems::HealingBeamView>& beams);

} // namespace Render::GL
