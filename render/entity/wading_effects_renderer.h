#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

// Waterline rings, ripples, wakes and spray around troops wading a river
// ford. Reads WadingComponent (FordSystem) and the formation's soldiers.
void render_wading_effects(Renderer* renderer,
                           ResourceManager* resources,
                           Engine::Core::World* world);

} // namespace Render::GL
