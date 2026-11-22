#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FrameSubmitter;
class DrawResources;

void render_wading_effects(FrameSubmitter* renderer,
                           const DrawResources* resources,
                           Engine::Core::World* world);

} // namespace Render::GL
