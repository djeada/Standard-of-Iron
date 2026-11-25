#pragma once

#include <functional>

namespace Engine::Core {
class World;
}

namespace Render::Pass {
class FramePassRunner;
}

namespace Render::GL {

class Renderer;

struct WorldFrameSources {
  Engine::Core::World* world = nullptr;

  std::function<void(Renderer&)> terrain;

  std::function<void(Renderer&)> overlays;
};

void build_world_frame_passes(Renderer& renderer,
                              Render::Pass::FramePassRunner& frame,
                              Render::Pass::FramePassRunner& world);

} // namespace Render::GL
