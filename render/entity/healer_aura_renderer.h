#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FrameSubmitter;
class DrawResources;

void render_healer_auras(FrameSubmitter* renderer,
                         const DrawResources* resources,
                         Engine::Core::World* world);

} // namespace Render::GL
