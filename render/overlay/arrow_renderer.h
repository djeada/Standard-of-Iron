#pragma once

#include <vector>

namespace Game::Systems {
struct ArrowInstance;
}

namespace Render::GL {

class FrameSubmitter;
class DrawResources;

void render_arrows(FrameSubmitter* renderer,
                   const DrawResources* resources,
                   const std::vector<Game::Systems::ArrowInstance>& arrows);

} // namespace Render::GL
