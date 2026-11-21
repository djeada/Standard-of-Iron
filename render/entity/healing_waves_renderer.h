#pragma once

#include <vector>

namespace Game::Systems {
struct HealingBeamView;
}

namespace Render::GL {
class FrameSubmitter;
class ResourceManager;

void render_healing_waves(FrameSubmitter* renderer,
                          ResourceManager* resources,
                          const std::vector<Game::Systems::HealingBeamView>& beams);

} // namespace Render::GL
