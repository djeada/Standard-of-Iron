#pragma once

#include <vector>

namespace Game::Systems {
struct ArrowInstance;
}

namespace Render::GL {

class FrameSubmitter;
class ResourceManager;

void render_arrows(FrameSubmitter* renderer,
                   ResourceManager* resources,
                   const std::vector<Game::Systems::ArrowInstance>& arrows);

} // namespace Render::GL
