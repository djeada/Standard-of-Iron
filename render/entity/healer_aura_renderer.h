#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

void render_healer_auras(Renderer *renderer, ResourceManager *resources,
                         Engine::Core::World *world);

} // namespace Render::GL
