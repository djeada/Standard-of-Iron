#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

void render_combat_dust(Renderer *renderer, ResourceManager *resources,
                        Engine::Core::World *world);

} // namespace Render::GL
