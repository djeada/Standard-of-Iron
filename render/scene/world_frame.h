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

// What a host (the game, the arena, a preview) hands the scene renderer to
// build one world frame. The host owns the terrain scene and the overlay
// effects; the renderer owns the order they run in.
struct WorldFrameSources {
  Engine::Core::World* world = nullptr;
  // Terrain, water, scatter and atmosphere, submitted before the world walk.
  std::function<void(Renderer&)> terrain;
  // Effects drawn over the world (projectiles, beams, markers), after it.
  std::function<void(Renderer&)> overlays;
};

// The one place the frame order is written down. `frame` receives the stages
// of a whole world frame; `world` receives the stages of the world walk, which
// run inside it while the render snapshot is locked.
void build_world_frame_passes(Renderer& renderer,
                              Render::Pass::FramePassRunner& frame,
                              Render::Pass::FramePassRunner& world);

} // namespace Render::GL
