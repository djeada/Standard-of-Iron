#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FrameSubmitter;
class ResourceManager;

void render_commander_auras(FrameSubmitter* renderer,
                            ResourceManager* resources,
                            Engine::Core::World* world);

} // namespace Render::GL
