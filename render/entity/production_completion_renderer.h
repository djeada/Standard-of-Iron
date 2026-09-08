#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;

void render_production_completions(Renderer* renderer,
                                   Engine::Core::World* world,
                                   int local_owner_id,
                                   bool reduced_motion);

} // namespace Render::GL
